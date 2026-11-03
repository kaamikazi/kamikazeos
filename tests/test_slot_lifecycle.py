"""Disposable-QEMU control for per-module lifecycle and scoped reset."""
import kmz_disk as disk
from kmz_qemu import section_kv
from test_integrity_gate import KmzCase


class T5SlotLifecycle(KmzCase):
    def test_two_module_lifecycle_and_explicit_reset_survive_reboot(self):
        for i, (pid, module, old, new) in enumerate((
                (201, "GhostAttach", "0.3", "0.4"),
                (301, "AI Recovery Core Rules", "0.4", "0.5"))):
            disk.write_manifest(self.img, i, disk.fixture_manifest_sector(
                f"PKG-{pid}", f"slot_scope_{pid}", "Disposable slot lifecycle fixture",
                "Slot fixture", old, new, target_module=module))
        with self.boot() as q:
            for pid in (201,301):
                for verb, code in (("approve-pkg",1), ("apply-pkg",2), ("verify-pkg",3)):
                    q.run(f"{verb} PKG-{pid}", rf"code=PKG-ACT-00{code}")
            report = self.report(q)
            for pid in (201,301):
                self.assertEqual(self.rows(report,"RECOVERY_PACKAGES")[f"PKG-{pid}"]["status"], "verified")
        with self.boot() as q:
            ai_before = disk.slots_sector(self.img)[50:72]
            q.run("rollback-pkg PKG-201", r"code=PKG-RB-001")
            self.assertEqual(disk.slots_sector(self.img)[50:72], ai_before)
            q.run("rollback-pkg PKG-301", r"code=PKG-RB-001")
            journal_before = disk.journal_bytes(self.img)
            q.run("reset-packages confirm", r"code=PST-100")
            self.assertEqual(disk.journal_bytes(self.img), journal_before)
        with self.boot() as q:
            report = self.report(q)
            self.assertEqual(section_kv(report,"PACKAGE_STATE_INTEGRITY")["writes_allowed"], "yes")
            q.run("approve-pkg PKG-201", r"code=PKG-ACT-001")
            q.run("apply-pkg PKG-201", r"code=PKG-ACT-002")
