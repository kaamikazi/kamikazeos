"""Questionable package copies must not resurrect retired ownership."""
from test_integrity_gate import KmzCase, PKG_ACT_OR_BLOCK
import kmz_disk as disk
from kmz_qemu import QemuSession, section_kv


class R6UncertainOwnership(KmzCase):
    def prepare_cycle(self):
        for i, (before, after) in enumerate((("0.3", "0.4"), ("0.4", "0.3"), ("0.3", "0.4"))):
            disk.write_manifest(self.img, i, disk.fixture_manifest_sector(
                f"PKG-{201+i}", f"release_cycle_{i}", "Disposable ownership failure fixture",
                "GhostAttach", before, after))
        with self.boot() as q:
            for verb in ("approve-pkg", "apply-pkg", "verify-pkg"):
                q.run(f"{verb} PKG-201", PKG_ACT_OR_BLOCK)
        older = disk.read_sector(self.img, 48)
        with self.boot() as q:
            for pid in (202, 203):
                for verb in ("approve-pkg", "apply-pkg", "verify-pkg"):
                    q.run(f"{verb} PKG-{pid}", PKG_ACT_OR_BLOCK)
        return older

    def check_failure(self, damaged_lba, read_error=False):
        older = self.prepare_cycle()
        stale_lba = 49 if damaged_lba == 48 else 48
        disk.write_sector(self.img, stale_lba, older)
        if not read_error:
            bad = bytearray(disk.read_sector(self.img, damaged_lba))
            bad[6 + bad[5] * 37] ^= 1
            disk.write_sector(self.img, damaged_lba, bytes(bad))
        self.assert_held((damaged_lba,) if read_error else ())

    def assert_held(self, read_errors=()):
        before = {lba: disk.read_sector(self.img, lba) for lba in (48, 49, 56)}
        prefix = disk.journal_bytes(self.img)[:disk.journal_used_slots(self.img) * 16]
        with QemuSession(self.boot().elf, self.img, self.log,
                         read_error_lbas=read_errors) as q:
            for command in ("rollback-pkg PKG-201", "verify-pkg PKG-203",
                            "approve-pkg PKG-201", "apply-pkg PKG-201",
                            "reset-packages confirm"):
                q.run(command)
            report = self.report(q)
        for lba, raw in before.items():
            self.assertEqual(disk.read_sector(self.img, lba), raw,
                             "uncertain ownership authorized package metadata writes")
        self.assertEqual(disk.journal_bytes(self.img)[:len(prefix)], prefix)
        self.assertEqual(section_kv(report, "PACKAGE_STATE_INTEGRITY")["ownership_state"], "uncertain")
        self.assertEqual(section_kv(report, "PACKAGE_STATE_INTEGRITY")["writes_allowed"], "no")

    def test_newer_mirror_read_error_holds_all_mutations(self): self.check_failure(49, True)
    def test_newer_primary_read_error_holds_all_mutations(self): self.check_failure(48, True)
    def test_newer_mirror_checksum_error_holds_all_mutations(self): self.check_failure(49)
    def test_newer_primary_checksum_error_holds_all_mutations(self): self.check_failure(48)

    def test_missing_mirror_does_not_authorize_stale_primary(self):
        older = self.prepare_cycle()
        disk.write_sector(self.img, 48, older)
        disk.write_sector(self.img, 49, bytes(512))
        self.assert_held()

    def test_missing_primary_does_not_authorize_stale_mirror(self):
        older = self.prepare_cycle()
        disk.write_sector(self.img, 49, older)
        disk.write_sector(self.img, 48, bytes(512))
        self.assert_held()

    def sidecar_failure(self, offset):
        older = self.prepare_cycle()
        disk.write_sector(self.img, 48, older)
        disk.corrupt_byte(self.img, 49, offset)
        self.assert_held()

    def test_damaged_sidecar_checksum_is_not_legacy_fallback(self): self.sidecar_failure(354)
    def test_damaged_sidecar_magic_is_not_legacy_fallback(self): self.sidecar_failure(320)

    def test_erased_state_beside_active_slots_is_not_fresh(self):
        self.prepare_cycle()
        for lba in (48, 49): disk.write_sector(self.img, lba, bytes(512))
        self.assert_held()

    def test_legacy_package_records_remain_unchanged_and_unbound(self):
        older = bytearray(self.prepare_cycle())
        used = 6 + older[5] * 37
        older[used + 1:] = bytes(512 - used - 1)
        for lba in (48, 49): disk.write_sector(self.img, lba, bytes(older))
        self.assert_held()

    def test_interrupted_first_state_write_is_not_automatically_repaired(self):
        with QemuSession(self.boot().elf, self.img, self.log, write_error_lbas=(48,)) as q:
            q.run("approve-pkg PKG-001", r"code=PKG-ACT-001")
            report = self.report(q)
        self.assertEqual(section_kv(report, "PACKAGE_STATE_INTEGRITY")["ownership_state"],
                         "degraded_current")
        self.assertEqual(disk.read_sector(self.img, 48), bytes(512))
        self.assertTrue(any(disk.read_sector(self.img, 49)))
        self.assert_held()

    def test_degraded_current_apply_and_rollback_survive_reboot(self):
        self.prepare_cycle()
        # Reapply through a legitimate rollback: active 0.4 -> 0.3, then
        # a fresh PKG-203 apply writes only the mirror during this session.
        with self.boot() as q:
            q.run("rollback-pkg PKG-203", r"code=PKG-RB-001")
            q.run("approve-pkg PKG-203", r"code=PKG-ACT-001")
        with QemuSession(self.boot().elf, self.img, self.log, write_error_lbas=(48,)) as q:
            q.run("apply-pkg PKG-203", r"code=PKG-ACT-002")
            report = self.report(q)
            self.assertEqual(section_kv(report, "PACKAGE_STATE_INTEGRITY")["ownership_state"],
                             "degraded_current")
        with self.boot() as q:
            report = self.report(q)
            self.assertEqual(section_kv(report, "PACKAGE_STATE_INTEGRITY")["ownership_state"], "verified")
            q.run("rollback-pkg PKG-203", r"code=PKG-RB-001")

    def test_staged_apply_with_bad_peer_preserves_intent_without_reconciliation(self):
        self.prepare_cycle()
        raw = bytearray(disk.read_sector(self.img, 49))
        record = 6 + 2 * 37
        raw[record + 10:record + 16] = bytes((2, 1, 0, 0, 0, 0))
        raw[330 + 2 * 8 + 5] = 2
        raw[330 + 2 * 8 + 6] = 2
        disk.write_sector(self.img, 49, disk.resign_package_state(bytes(raw), 500))
        disk.corrupt_byte(self.img, 48, 6 + 3 * 37)
        slot = bytearray(disk.slots_sector(self.img))
        slot[25] = 0  # GhostAttach B is unverified
        slot[27] = 1  # slot switch happened, verification is pending
        slot[94] = disk.sum_byte(slot[:94])
        disk.write_sector(self.img, 56, bytes(slot))
        self.assert_held()

    def test_read_fault_cleared_on_reboot_restores_current_owner(self):
        self.prepare_cycle()
        self.assert_held((49,))
        with self.boot() as q:
            report = self.report(q)
            self.assertEqual(section_kv(report, "PACKAGE_STATE_INTEGRITY")["ownership_state"], "verified")
            q.run("rollback-pkg PKG-203", r"code=PKG-RB-001")
