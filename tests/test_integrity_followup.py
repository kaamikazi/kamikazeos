"""Additional audit regressions; all disk mutations use disposable images."""
from test_integrity_gate import KmzCase, PKG_ACT_OR_BLOCK
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
