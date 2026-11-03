"""Structural entry checks and read-only descriptor inspection; never trigger DF."""
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import time
import unittest
from kmz_qemu import section_kv
from test_integrity_gate import KmzCase
from test_memory_layout import PREFIXES, elf_layout

class B4EntryELF(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.elf=Path(os.environ["KMZ_TEST_ELF"])
        _,_,cls.symbols=elf_layout(cls.elf)
        objdump=shutil.which("llvm-objdump") or r"C:\Program Files\LLVM\bin\llvm-objdump.exe"
        p=subprocess.run([objdump,"--disassemble","--no-show-raw-insn","--x86-asm-syntax=intel",str(cls.elf)],
                         capture_output=True,text=True,check=True)
        cls.instructions=[(int(a,16),text.strip().lower()) for a,text in
                          re.findall(r"^\s*([0-9a-f]+):\s+(.+)$",p.stdout,re.M)]
        directory=Path(os.environ["KMZ_TEST_RUN_DIR"])/"emergency-entry-static"
        directory.mkdir(exist_ok=True);(directory/"disassembly.txt").write_text(p.stdout+p.stderr)

    def body(self,name):
        start,size=self.symbols[name]
        self.assertGreater(size,0,name)
        return [text for at,text in self.instructions if start<=at<start+size]

    def test_fatal_entry_does_not_use_old_stack_before_switch(self):
        for name in ("kpanic",*(f"isr{i}" for i in range(32))):
            body=self.body(name)
            self.assertTrue(any("bts" in line for line in body),name)
            self.assertFalse(any(re.match(r"(?:push|pop|call|ret|iret)",line) or "[esp" in line
                                 or "[ebp" in line for line in body),name)
        common=self.body("emergency_exception_common")
        switch=next(i for i,line in enumerate(common) if re.match(r"mov\s+esp,",line))
        self.assertFalse(any(re.match(r"(?:push|pop|call)",line) or "[esp" in line or "[ebp" in line
                             for line in common[:switch]))
        self.assertTrue(any(re.match(r"call",line) for line in common[switch+1:]))

    def test_emergency_entries_are_nonreturning_and_reentry_is_stackless(self):
        for name in ("emergency_exception_common","kpanic","emergency_reentry_capture",
                     "emergency_reentry_halt","emergency_double_fault_entry"):
            self.assertFalse(any(re.match(r"(?:ret|iret)",line) for line in self.body(name)),name)
        for name in ("emergency_reentry_capture","emergency_reentry_halt"):
            self.assertFalse(any(re.match(r"(?:push|pop|call)",line) or "[esp" in line for line in self.body(name)),name)
        self.assertTrue(any(re.match(r"hlt",line) for line in self.body("emergency_reentry_halt")))

def monitor_text(q,start):
    text=b"".join(q.monitor_out[start:]).decode("ascii","replace")
    return re.sub(r"\x1b\[[0-?]*[ -/]*[@-~]","",text)

def monitor_words(q,base,count):
    start=len(q.monitor_out);q.monitor(f"xp /{count}wx 0x{base:x}")
    deadline=time.monotonic()+10
    while time.monotonic()<deadline:
        values={}
        for line in monitor_text(q,start).splitlines():
            m=re.search(r"([0-9a-fA-F]{8,16}):\s+((?:0x[0-9a-fA-F]{8}\s*)+)",line)
            if not m:continue
            address=int(m[1],16)
            for i,value in enumerate(re.findall(r"0x([0-9a-fA-F]{8})",m[2])):
                at=address+i*4
                if base<=at<base+count*4:values[at]=int(value,16)
        if len(values)==count:return [values[base+i*4] for i in range(count)]
        time.sleep(.05)
    raise TimeoutError(f"Incomplete read-only monitor dump at {base:x}")

class B5EmergencyBoot(KmzCase):
    def test_prepared_df_architecture_is_structural_only_and_normal_tr_is_preserved(self):
        q=self.boot()
        try:
            with q:
                report=self.report(q);arch=section_kv(report,"EMERGENCY_ARCH")
                for key in ("runtime_ready","df_prepared","df_descriptor_valid","df_gate_valid","df_state_valid"):
                    self.assertEqual(arch[key],"yes",key)
                self.assertEqual(arch["record_bytes"],"128")
                self.assertEqual(arch["entry_abi_bytes"],"100")
                self.assertEqual(arch["normal_tss_selector"],"40")
                self.assertEqual(arch["df_tss_selector"],"48")
                for key in ("first_record_magic","df_record_magic","reentry_record_magic","entry_busy","reentry_count"):
                    self.assertEqual(arch[key],"0",key)
                self.assertEqual(arch["real_fault_path_validation"],"deferred")
                self.assertEqual(arch["intentional_fault_tests_approved"],"no")
                self.assertEqual(arch["hardware_task_scheduling"],"no")
                _,_,symbols=elf_layout(q.elf)
                gate=monitor_words(q,symbols["idt"][0]+8*8,2)
                self.assertEqual(struct.pack("<2I",*gate),b"\0\0\x30\0\0\x85\0\0")
                gdt=monitor_words(q,symbols["gdt"][0],14)
                raw=struct.pack("<14I",*gdt)
                self.assertEqual(raw[5*8+5],0x8B) # CPU marked only normal TSS busy via LTR
                self.assertEqual(raw[6*8+5],0x89) # DF task is available, never entered
                lo,base_lo,mid,access,gran,high=struct.unpack_from("<HHBBBB",raw,6*8)
                self.assertEqual((lo,gran),(103,0))
                base=base_lo|(mid<<16)|(high<<24)
                self.assertEqual(base,int(arch["df_tss_address"]))
                task=monitor_words(q,base,26)
                self.assertEqual(task[0],0)
                self.assertEqual(task[7],int(section_kv(report,"PAGING")["active_cr3"]))
                self.assertEqual(task[8],symbols["emergency_double_fault_entry"][0])
                self.assertEqual(task[9],2)
                self.assertEqual(task[1],int(arch["df_stack_end"]))
                self.assertEqual(task[14],task[1])
                self.assertEqual(task[19],8)
                for index in (2,18,20,21,22,23):self.assertEqual(task[index],16)
                self.assertEqual(task[25],104<<16)
                # Read actual live paging structures, independent of report/model code.
                directory=monitor_words(q,symbols["page_directory"][0],1024)
                code=symbols["_user_code_start"][0]//4096
                stack=set(range(symbols["_user_stack_start"][0]//4096,
                                symbols["_user_stack_end"][0]//4096))
                user={code}|stack
                self.assertEqual(len(user),5)
                leaves=[]
                for table in range(4):
                    self.assertEqual(directory[table]&0xFFFFF000,
                                     symbols["identity_tables"][0]+table*4096)
                    self.assertEqual(directory[table]&7,7 if any(p//1024==table for p in user) else 3)
                    leaves.extend(monitor_words(q,directory[table]&0xFFFFF000,1024))
                self.assertTrue(all(value==0 for value in directory[4:]))
                for page,value in enumerate(leaves):
                    self.assertEqual(value&0xFFFFF000,page*4096)
                    self.assertEqual(value&7,5 if page==code else 7 if page in stack else 3)
                guards=[p for p in PREFIXES if "guard" in p]
                self.assertEqual(len(guards),10)
                for prefix in guards:
                    self.assertEqual(leaves[symbols[prefix+"_start"][0]//4096]&7,3,prefix)
                self.assertEqual(leaves[0]&7,3) # NULL protection remains inactive.
                start=len(q.monitor_out);q.monitor("info registers")
                deadline=time.monotonic()+10
                registers=""
                while time.monotonic()<deadline:
                    registers=monitor_text(q,start)
                    if re.search(r"CR4=[0-9A-Fa-f]{8}",registers):break
                    time.sleep(.05)
                cr0=int(re.search(r"CR0=([0-9A-Fa-f]{8})",registers)[1],16)
                cr4=int(re.search(r"CR4=([0-9A-Fa-f]{8})",registers)[1],16)
                self.assertFalse(cr0&0x10000)
                self.assertFalse(cr4&0x20)
                self.assertRegex(registers,r"TR\s*=\s*0028")
                self.assertEqual(section_kv(report,"RING3")["execution_enabled"],"no")
        finally:
            (self.dir/"monitor.log").write_bytes(b"".join(q.monitor_out))
