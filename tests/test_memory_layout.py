"""Read-only ELF layout checks and ordinary boot; no new fault entry points."""
import os
from pathlib import Path
import struct
import unittest
from kmz_qemu import QemuSession, section_kv
from test_integrity_gate import KmzCase

PREFIXES = (
    "_kernel_text", "_kernel_rodata", "_kernel_data", "_kernel_bss", "_page_directory", "_page_tables",
    "_bootstrap_guard_low", "_bootstrap_stack", "_bootstrap_guard_high",
    "_privilege_guard_low", "_privilege_stack", "_privilege_guard_high",
    "_user_code", "_user_guard_low", "_user_stack", "_user_guard_high", "_heap_candidate",
)

def elf_layout(path):
    data = Path(path).read_bytes()
    if data[:6] != b"\x7fELF\x01\x01": raise ValueError("expected little-endian ELF32")
    off = struct.unpack_from("<I", data, 32)[0]
    size, count, names = struct.unpack_from("<HHH", data, 46)
    sections = [struct.unpack_from("<10I", data, off + i * size) for i in range(count)]
    def blob(s): return data[s[4]:s[4] + s[5]]
    strings = blob(sections[names])
    def text(buf, at): return buf[at:buf.index(b"\0", at)].decode()
    by_name = {text(strings, s[0]): s for s in sections}
    symtab = by_name[".symtab"]
    strings = blob(sections[symtab[6]])
    symbols = {}
    for at in range(symtab[4], symtab[4] + symtab[5], symtab[9]):
        name, value, length, info, other, section = struct.unpack_from("<IIIBBH", data, at)
        symbols[text(strings, name)] = (value, length)
    return data, by_name, symbols

class M1LinkedLayout(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data, cls.sections, cls.symbols = elf_layout(os.environ["KMZ_TEST_ELF"])

    def test_all_primary_bounds_are_aligned_ordered_and_inside_aperture(self):
        previous = 0x100000
        for prefix in PREFIXES:
            start, end = (self.symbols[prefix + suffix][0] for suffix in ("_start", "_end"))
            self.assertEqual(start, previous, prefix)
            self.assertGreater(end, start, prefix)
            self.assertEqual((start | end) & 4095, 0, prefix)
            self.assertLessEqual(end, 0x1000000, prefix)
            previous = end

    def test_user_payloads_guards_and_actual_objects_have_exact_bounds(self):
        s = self.symbols
        for a, b in (("stack_bottom", "_bootstrap_stack_start"), ("stack_top", "_bootstrap_stack_end"),
                     ("ring3_user_stack_start", "_user_stack_start"), ("ring3_user_stack_end", "_user_stack_end")):
            self.assertEqual(s[a][0], s[b][0])
        self.assertEqual(s["_user_code_end"][0] - s["_user_code_start"][0], 4096)
        self.assertEqual(s["_user_stack_end"][0] - s["_user_stack_start"][0], 16384)
        for prefix in (p for p in PREFIXES if "guard" in p):
            self.assertEqual(s[prefix + "_end"][0] - s[prefix + "_start"][0], 4096)
        for name, prefix, size in (("page_directory", "_page_directory", 4096),
                                   ("identity_tables", "_page_tables", 16384),
                                   ("privilege_stack", "_privilege_stack", 16384)):
            self.assertEqual(s[name], (s[prefix + "_start"][0], size))
        self.assertEqual(s["_kernel_end"][0], s["_user_guard_high_end"][0])
        self.assertEqual(s["_heap_candidate_start"][0], s["_kernel_end"][0])

    def test_no_allocatable_orphans_and_rodata_strings_are_collected(self):
        expected = {".text", ".rodata", ".data", ".bss", ".page_directory", ".page_tables",
                    ".bootstrap_stack", ".privilege_stack", ".usertext", ".userstack"}
        actual = {n for n, s in self.sections.items() if s[2] & 2}
        self.assertEqual(actual, expected)
        self.assertNotIn(".rodata.str1.1", self.sections)
        for name in expected:
            s = self.sections[name]
            self.assertEqual(s[3] & 4095, 0)
            self.assertEqual(s[5] & 4095, 0)
        for name in (".text", ".rodata", ".usertext"):
            self.assertEqual(self.sections[name][2] & 1, 0)  # ELF intent only, not PTE/NX enforcement
        text = self.sections[".text"]
        self.assertEqual(struct.unpack_from("<I", self.data, text[4])[0], 0x1BADB002)
        self.assertLess(text[4], 8192)

class M1LayoutBoot(KmzCase):
    def test_linked_layout_boots_with_legacy_paging_and_ring3_held(self):
        with self.boot() as q:
            q.run("heap-test")
            q.run("task-demo")
            report = self.report(q)
        pg = section_kv(report, "PAGING")
        self.assertEqual(pg["enabled"], "yes")
        self.assertEqual(pg["identity_map_valid"], "yes")
        self.assertEqual(pg["mapped_end"], str(16 * 1024 * 1024))
        user = section_kv(report, "USER_MEMORY")
        self.assertEqual(user["validated"], "yes")
        self.assertEqual(user["no_broad_user_exposure"], "yes")
        self.assertEqual(section_kv(report, "RING3")["execution_enabled"], "no")
        policy = section_kv(report, "MEMORY_POLICY")
        self.assertEqual(policy["layout_valid"], "yes")
        self.assertEqual(policy["protected_plan_installed"], "no")
        self.assertEqual(policy["guard_pages_active"], "0")
        self.assertEqual(policy["cr0_wp_configured_by_phase_a"], "no")
        self.assertEqual(policy["page_structures_mutable"], "yes")
        self.assertEqual(policy["nx_enforced"], "no")
        rows = {r["name"]: r for r in report["MEMORY_REGIONS"]}
        self.assertEqual(len(rows), 21)
        for name in ("text", "rodata", "page_directory", "page_tables"):
            self.assertEqual(rows[name]["legacy_flags"], "3")
            self.assertEqual(rows[name]["plan_flags_if_ram"], "1")
        for name in (n for n in rows if "guard" in n):
            self.assertEqual(rows[name]["legacy_flags"], "3")
            self.assertEqual(rows[name]["plan_flags_if_ram"], "0")

    def test_ram_profiles_report_bounded_shadow_eligibility_without_activation(self):
        for mib in (4,16,128):
            with self.subTest(memory_mib=mib):
                with QemuSession(self.boot().elf, self.img, self.log, memory_mib=mib) as q:
                    report = self.report(q)
                ram = section_kv(report, "RAM_ACCOUNTING")
                self.assertEqual(ram["valid"], "yes")
                self.assertEqual(ram["discovery_trusted"], "yes")
                self.assertEqual(ram["mapping_active"], "yes")
                self.assertEqual(ram["mapped_present_pages"], "4096")
                self.assertEqual(ram["eligibility_enforced"], "no")
                usable = int(ram["mapped_usable_pages"])
                self.assertEqual(usable + int(ram["mapped_nonram_pages"]), 4096)
                self.assertGreater(int(ram["eligible_pages"]), 0)
                self.assertLessEqual(int(ram["eligible_pages"]), usable)
                if mib > 16:
                    self.assertGreater(int(ram["unmapped_usable_pages"]), 0)
                else:
                    self.assertEqual(ram["unmapped_usable_pages"], "0")
                self.assertEqual(section_kv(report, "RING3")["execution_enabled"], "no")
