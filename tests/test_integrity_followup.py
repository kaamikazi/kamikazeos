"""Additional audit regressions; all disk mutations use disposable images."""
from test_integrity_gate import KmzCase
import kmz_disk as disk
from kmz_qemu import section_kv


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
