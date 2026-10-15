"""Additional audit regressions; all disk mutations use disposable images."""
from pathlib import Path
import unittest

from test_integrity_gate import BASELINE_ELF, KmzCase, PKG_ACT_OR_BLOCK, RB_RESULT
import kmz_disk as disk
from kmz_qemu import events, section_kv


class N1LegacyClassification(KmzCase):
    def test_damaged_extension_magic_cannot_impersonate_legacy_writer(self):
        self.boot_once()
        primary = disk.read_sector(self.img, 1)
        prefix = disk.journal_bytes(self.img)[:disk.journal_used_slots(self.img) * 16]
        for offset in range(32, 36):
            with self.subTest(magic_byte=offset):
                # A checksum-valid base claiming different flags/boot order,
                # but with a damaged modern extension, is not legacy state.
                damaged = bytearray(disk.resign_state(primary, boot_count=80))
                damaged[offset] ^= 1
                disk.write_sector(self.img, 1, bytes(damaged))
                disk.write_sector(self.img, 50, primary)
                report = self.boot_once()
                self.assertEqual(section_kv(report, "RECOVERY_STORE")["state_source"], "mirror")
                self.assertEqual(section_kv(report, "RECOVERY_MEMORY")["boot_count"],
                                 str(primary[5] + 1))
                self.assertEqual(disk.journal_bytes(self.img)[:len(prefix)], prefix)
                primary = disk.read_sector(self.img, 1)

    def test_nonzero_reserved_tail_is_not_a_legacy_state(self):
        self.boot_once()
        disk.write_legacy_state(self.img, boot_count=80,
                                jcount=disk.journal_used_slots(self.img))
        disk.corrupt_byte(self.img, 1, 100)
        report = self.boot_once()
        self.assertEqual(section_kv(report, "RECOVERY_STORE")["state_source"], "mirror")
        self.assertEqual(section_kv(report, "RECOVERY_MEMORY")["boot_count"], "2")


class N2GenerationOrdering(KmzCase):
    def test_half_range_recovery_generations_preserve_all_evidence(self):
        for gp, gm in ((1, 0x80000001), (0x80000001, 1)):
            with self.subTest(primary=gp, mirror=gm):
                disk.blank_image(self.img)
                self.boot_once()
                state = disk.read_sector(self.img, 1)
                disk.write_sector(self.img, 1, disk.resign_state(state, generation=gp))
                disk.write_sector(self.img, 50, disk.resign_state(state, generation=gm))
                before = {lba: disk.read_sector(self.img, lba)
                          for lba in (1, 50, *range(2, 32))}
                report = self.boot_once()
                store = section_kv(report, "RECOVERY_STORE")
                self.assertEqual(store["state_locked"], "1")
                self.assertEqual(store["journal_locked"], "1")
                for lba, raw in before.items():
                    self.assertEqual(disk.read_sector(self.img, lba), raw)

    def test_recovery_normal_ordering_and_wraparound(self):
        for gp, gm, source in ((100, 101, "mirror"),
                               (0xffffffff, 0, "mirror"),
                               (0, 0xffffffff, "primary")):
            with self.subTest(primary=gp, mirror=gm):
                disk.blank_image(self.img)
                self.boot_once()
                state = disk.read_sector(self.img, 1)
                disk.write_sector(self.img, 1, disk.resign_state(state, generation=gp))
                disk.write_sector(self.img, 50, disk.resign_state(state, generation=gm))
                report = self.boot_once()
                store = section_kv(report, "RECOVERY_STORE")
                self.assertEqual(store["state_source"], source)
                self.assertEqual(store["state_locked"], "0")
                self.assertEqual(section_kv(report, "RECOVERY_MEMORY")["boot_count"], "2")

    def package_history(self):
        with self.boot() as q:
            for verb in ("approve-pkg", "apply-pkg"):
                q.run(f"{verb} PKG-001", PKG_ACT_OR_BLOCK)

    def check_package_ambiguity(self, gp, gm, conflict=False):
        disk.blank_image(self.img)
        self.package_history()
        primary = disk.read_sector(self.img, 48)
        mirror = bytearray(primary)
        if conflict:
            mirror[42] ^= 1  # health baseline of record zero, base checksum resealed
        disk.write_sector(self.img, 48, disk.resign_package_state(primary, gp))
        disk.write_sector(self.img, 49, disk.resign_package_state(bytes(mirror), gm))
        before = {lba: disk.read_sector(self.img, lba) for lba in (48, 49, 56)}
        with self.boot() as q:
            for verb in ("approve-pkg", "apply-pkg", "verify-pkg", "rollback-pkg"):
                q.run(f"{verb} PKG-001")
            q.run("reset-packages confirm")
            self.report(q)
            text = q.boot_text()
        self.assertTrue(events(text, "PST-ERR-002"), "ambiguous package state not reported")
        for lba, raw in before.items():
            self.assertEqual(disk.read_sector(self.img, lba), raw,
                             "ambiguous package metadata was overwritten")

    def test_half_range_package_generations_are_read_only(self):
        for gp, gm in ((1, 0x80000001), (0x80000001, 1)):
            with self.subTest(primary=gp, mirror=gm):
                self.check_package_ambiguity(gp, gm)

    def test_equal_generation_conflicting_package_copies_are_read_only(self):
        self.check_package_ambiguity(40, 40, conflict=True)

    def test_package_generation_wrap_selects_newest_state(self):
        self.package_history()
        state = disk.read_sector(self.img, 48)
        # Older copy claims approval; newer wrapped copy records an apply.
        old = bytearray(state)
        old[16:22] = bytes((2, 1, 0, 0, 0, 0))
        disk.write_sector(self.img, 48, disk.resign_package_state(bytes(old), 0xffffffff))
        disk.write_sector(self.img, 49, disk.resign_package_state(state, 0))
        report = self.boot_once()
        self.assertEqual(self.rows(report, "RECOVERY_PACKAGES")["PKG-001"]["status"], "applied")
        with self.boot() as q:
            self.assertIn("PKG-ACT-003", q.run("verify-pkg PKG-001", PKG_ACT_OR_BLOCK))


class N3JournalValidation(KmzCase):
    def test_unknown_version_neither_exports_nor_suppresses_suggestion(self):
        with self.boot() as q:
            q.run("approve ACT-001", r"code=REC-ACT-003")
            q.run("rollback ACT-001", r"code=REC-RB-001")
        used = disk.journal_used_slots(self.img)
        bad = bytearray(disk.make_entry(250, disk.JE["SUGGESTED"], action=1, code="BADVER"))
        bad[1] = 2
        bad[15] = disk.sum_byte(bad[:15])
        disk.write_journal_slot(self.img, used, bytes(bad))
        prefix = disk.journal_bytes(self.img)[:(used + 1) * 16]
        report = self.boot_once()
        codes = [row["code"] for row in report["JOURNAL"]]
        with self.subTest(reader="report"):
            self.assertNotIn("BADVER", codes)
        self.assertEqual(section_kv(report, "RECOVERY_MEMORY")["boot_count"], "2")
        self.assertEqual(section_kv(report, "RECOVERY_STORE")["journal_corrupt_entries"], "1")
        self.assertEqual(disk.journal_bytes(self.img)[:len(prefix)], prefix)
        new_suggestions = [e for e in disk.journal_entries(self.img)[used + 1:]
                           if e[1] == 1 and e[3] == disk.JE["SUGGESTED"] and e[4] == 1]
        with self.subTest(reader="latest_action"):
            self.assertEqual(len(new_suggestions), 1, "invalid-version entry suppressed a new suggestion")

    def test_unknown_events_stay_occupied_but_are_not_exported(self):
        self.boot_once()
        used = disk.journal_used_slots(self.img)
        for offset, event in enumerate((0, 16, 255)):
            disk.write_journal_slot(self.img, used + offset,
                                    disk.make_entry(250, event, code=f"BADEV{offset}"))
        prefix = disk.journal_bytes(self.img)[:(used + 3) * 16]
        report = self.boot_once()
        self.assertFalse([row for row in report["JOURNAL"]
                          if row["code"].startswith("BADEV")])
        self.assertFalse(report["PACKAGE_HISTORY"], "unknown events exported as package history")
        self.assertEqual(section_kv(report, "RECOVERY_MEMORY")["boot_count"], "2")
        self.assertEqual(section_kv(report, "RECOVERY_STORE")["journal_corrupt_entries"], "3")
        self.assertEqual(disk.journal_bytes(self.img)[:len(prefix)], prefix)


class N4RollbackOwnership(KmzCase):
    def write_cycle(self):
        for index, (pid, before, after) in enumerate(((201, "0.3", "0.4"),
                                                     (202, "0.4", "0.3"),
                                                     (203, "0.3", "0.4"))):
            disk.write_manifest(self.img, index, disk.fixture_manifest_sector(
                f"PKG-{pid}", f"cycle_{pid}", "Disposable ABA ownership fixture",
                "GhostAttach", before, after))

    def apply_prefix(self, q):
        for pid in (201, 202):
            for verb in ("approve-pkg", "apply-pkg", "verify-pkg"):
                self.assertIn("PKG-ACT", q.run(f"{verb} PKG-{pid}", PKG_ACT_OR_BLOCK))

    def slots(self, report):
        return self.rows(report, "MODULE_SLOTS", "module")["GhostAttach"]

    def assert_owner(self, q):
        before = self.slots(self.report(q))
        self.assertIn("PKG-REJECT-001", q.run("rollback-pkg PKG-201", RB_RESULT))
        self.assertEqual(self.slots(self.report(q)), before)
        self.assertIn("PKG-RB-001", q.run("rollback-pkg PKG-203", RB_RESULT))
        after = self.slots(self.report(q))
        self.assertEqual(after["slot_a" if after["active"] == "A" else "slot_b"], "0.3")
        self.assertIn("PKG-REJECT-001", q.run("rollback-pkg PKG-202", RB_RESULT))
        self.assertEqual(self.slots(self.report(q)), after,
                         "rolling back the current owner resurrected an older owner")

    def test_aba_stale_owner_is_refused_without_restart(self):
        self.write_cycle()
        with self.boot() as q:
            self.apply_prefix(q)
            for verb in ("approve-pkg", "apply-pkg"):
                q.run(f"{verb} PKG-203", PKG_ACT_OR_BLOCK)
            self.assert_owner(q)

    def test_aba_ownership_survives_full_restart(self):
        self.write_cycle()
        with self.boot() as q:
            self.apply_prefix(q)
            for verb in ("approve-pkg", "apply-pkg", "verify-pkg"):
                q.run(f"{verb} PKG-203", PKG_ACT_OR_BLOCK)
        with self.boot() as q:
            self.assert_owner(q)

    @unittest.skipUnless(BASELINE_ELF, "KMZ_BASELINE_ELF not set")
    def test_retired_ownership_survives_tagged_downgrade_reupgrade(self):
        self.write_cycle()
        with self.boot() as q:
            self.apply_prefix(q)
            for verb in ("approve-pkg", "apply-pkg", "verify-pkg"):
                q.run(f"{verb} PKG-203", PKG_ACT_OR_BLOCK)
        before = {lba: disk.read_sector(self.img, lba) for lba in (48, 49, 56)}
        with self.boot(Path(BASELINE_ELF)) as q:
            self.assertEqual(self.rows(self.report(q), "RECOVERY_PACKAGES")["PKG-203"]["status"], "verified")
        for lba, raw in before.items():
            self.assertEqual(disk.read_sector(self.img, lba), raw)
        with self.boot() as q:
            self.assert_owner(q)

    def edit_package_record(self, index, *, before=None, status=None, phase=None):
        sec = bytearray(disk.read_sector(self.img, 48))
        record = 6 + index * 37
        tx = 330 + index * 8
        if before is not None:
            sec[record + 16] = before
        if status is not None:
            sec[record + 10:record + 16] = bytes((status, status == 2, status == 3,
                                                status == 4, status == 5, status == 6))
        if phase is not None:
            sec[tx + 5] = phase
            sec[tx + 6] = 2  # status before apply: approved
        generation = int.from_bytes(sec[326:330], "little")
        sealed = disk.resign_package_state(bytes(sec), generation)
        for lba in (48, 49):
            disk.write_sector(self.img, lba, sealed)

    def test_preexisting_ambiguous_owners_are_both_refused(self):
        self.write_cycle()
        with self.boot() as q:
            self.apply_prefix(q)
            for verb in ("approve-pkg", "apply-pkg"):
                q.run(f"{verb} PKG-203", PKG_ACT_OR_BLOCK)
        # Recreate old-writer state: two bound packages claim the identical
        # A->B, 0.3->0.4 transition. Neither is provably the latest owner.
        self.edit_package_record(0, before=0)
        before = disk.slots_sector(self.img)
        with self.boot() as q:
            for pid in (201, 203):
                self.assertIn("PKG-REJECT-001", q.run(f"rollback-pkg PKG-{pid}", RB_RESULT))
        self.assertEqual(disk.slots_sector(self.img), before)

    def test_aborted_staged_apply_keeps_previous_owner(self):
        self.write_cycle()
        with self.boot() as q:
            self.apply_prefix(q)
            q.run("approve-pkg PKG-203", PKG_ACT_OR_BLOCK)
        # A staged apply never switched module slots. Its abort must retain
        # PKG-202's legitimate B->A rollback authority.
        self.edit_package_record(2, before=0, phase=2)
        sec = bytearray(disk.read_sector(self.img, 48))
        sec[6 + 2 * 37 + 17] = 1  # staged destination is B
        for lba in (48, 49):
            disk.write_sector(self.img, lba, disk.resign_package_state(bytes(sec), 100))
        with self.boot() as q:
            self.assertTrue(events(q.boot_text(), "PST-RECOVER-ABO"))
            self.assertIn("PKG-RB-001", q.run("rollback-pkg PKG-202", RB_RESULT))
            active = self.slots(self.report(q))
            self.assertEqual(active["slot_b"], "0.4")
            self.assertEqual(active["active"], "B")

    def test_switched_staged_apply_retires_previous_owners(self):
        self.write_cycle()
        with self.boot() as q:
            self.apply_prefix(q)
            for verb in ("approve-pkg", "apply-pkg"):
                q.run(f"{verb} PKG-203", PKG_ACT_OR_BLOCK)
        # Slot switch durable, final package commit lost. Restore the prior
        # owner's rights as they were at the staged save, then reconcile.
        self.edit_package_record(1, before=1)
        self.edit_package_record(2, status=2, phase=2)
        with self.boot() as q:
            self.assertTrue(events(q.boot_text(), "PST-RECOVER-APP"))
            self.assert_owner(q)
