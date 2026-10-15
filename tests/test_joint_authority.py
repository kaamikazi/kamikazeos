"""Joint package/slot authority, including real staged-slot uncertainty."""
import struct
import kmz_disk as disk
from kmz_qemu import QemuSession, section_kv
from test_integrity_gate import KmzCase, PKG_ACT_OR_BLOCK

class T4JointAuthority(KmzCase):
    def prefix(self, finish=True):
        for i,(a,b) in enumerate((("0.3","0.4"),("0.4","0.3"),("0.3","0.4"))):
            disk.write_manifest(self.img,i,disk.fixture_manifest_sector(
                f"PKG-{201+i}",f"joint_{i}","Disposable joint-authority fixture","GhostAttach",a,b))
        stale=None
        with self.boot() as q:
            for pid in (201,202):
                for verb in ("approve-pkg","apply-pkg","verify-pkg"):
                    q.run(f"{verb} PKG-{pid}",PKG_ACT_OR_BLOCK)
                if pid==202: stale=disk.slots_sector(self.img)
            q.run("approve-pkg PKG-203",PKG_ACT_OR_BLOCK)
            if finish:
                for verb in ("apply-pkg","verify-pkg"): q.run(f"{verb} PKG-203",PKG_ACT_OR_BLOCK)
        return stale

    def failure(self,kind):
        stale=self.prefix()
        if kind=="checksum": disk.corrupt_byte(self.img,56,94)
        if kind=="empty": disk.write_sector(self.img,56,bytes(512))
        if kind=="stale": disk.write_sector(self.img,56,stale)
        before={lba:disk.read_sector(self.img,lba) for lba in (48,49,56)}
        with QemuSession(self.boot().elf,self.img,self.log,read_error_lbas=(56,) if kind=="read" else ()) as q:
            q.run("approve-pkg PKG-201")
            q.run("apply-pkg PKG-201")
            q.run("reset-packages confirm")
            report=self.report(q)
        for lba,raw in before.items(): self.assertEqual(disk.read_sector(self.img,lba),raw)
        self.assertEqual(section_kv(report,"PACKAGE_STATE_INTEGRITY")["writes_allowed"],"no")
        if kind=="read":
            with self.boot() as q: q.run("rollback-pkg PKG-203",r"code=PKG-RB-001")

    def test_slot_read_failure_preserves_authority_evidence(self): self.failure("read")
    def test_slot_checksum_failure_preserves_authority_evidence(self): self.failure("checksum")
    def test_erased_slots_do_not_establish_factory_authority(self): self.failure("empty")
    def test_stale_valid_slots_do_not_override_current_owner(self): self.failure("stale")

    def test_staged_apply_is_preserved_until_slot_reads_recover(self):
        self.prefix(False)
        s=bytearray(disk.read_sector(self.img,48)); r=6+2*37; tx=330+2*8
        s[r+16]=0;s[r+17]=1;s[r+35]=0;s[r+36]=80;s[tx+5]=2;s[tx+6]=2
        s=disk.resign_package_state(bytes(s),500)
        for lba in (48,49): disk.write_sector(self.img,lba,s)
        slot=bytearray(disk.slots_sector(self.img));slot[23]=1;slot[25]=0;slot[26]=0;slot[27]=1
        slot[94]=disk.sum_byte(slot[:94]);disk.write_sector(self.img,56,bytes(slot))
        before={lba:disk.read_sector(self.img,lba) for lba in (48,49,56)}
        with QemuSession(self.boot().elf,self.img,self.log,read_error_lbas=(56,)) as q:
            report=self.report(q)
        for lba,raw in before.items(): self.assertEqual(disk.read_sector(self.img,lba),raw)
        self.assertEqual(section_kv(report,"PACKAGE_STATE_INTEGRITY")["writes_allowed"],"no")
        with self.boot() as q:
            report=self.report(q)
            self.assertEqual(self.rows(report,"RECOVERY_PACKAGES")["PKG-203"]["status"],"applied")
            q.run("rollback-pkg PKG-203",r"code=PKG-RB-001")

    def test_registry_reload_cannot_clear_joint_slot_hold(self):
        stale=self.prefix();disk.write_sector(self.img,56,stale)
        before={lba:disk.read_sector(self.img,lba) for lba in (48,49,56)}
        with self.boot() as q:
            q.run("seed-manifests confirm",r"code=MAN-00[23]")
            q.run("approve-pkg PKG-201")
            q.run("apply-pkg PKG-201")
            report=self.report(q)
        for lba,raw in before.items(): self.assertEqual(disk.read_sector(self.img,lba),raw)
        self.assertEqual(section_kv(report,"PACKAGE_STATE_INTEGRITY")["writes_allowed"],"no")
