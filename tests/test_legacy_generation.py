"""Legacy-primary generation regression on a disposable QEMU image."""
from test_integrity_gate import KmzCase
import kmz_disk as disk
from kmz_qemu import section_kv


class L1LegacyGeneration(KmzCase):
    def test_legacy_primary_keeps_newer_mirror_generation(self):
        self.boot_once()
        mirror = disk.read_sector(self.img, 50)
        disk.write_sector(self.img, 50, disk.resign_state(mirror, generation=500))
        disk.write_legacy_state(self.img, boot_count=30,
                                jcount=disk.journal_used_slots(self.img))
        self.boot_once()
        self.assertEqual(disk.state_fields(self.img)["boot_count"], 31)
        self.assertGreater(disk.state_generation(disk.read_sector(self.img, 1)), 500)
        self.assertEqual(disk.read_sector(self.img, 1), disk.read_sector(self.img, 50))
        rep = self.boot_once()
        self.assertEqual(section_kv(rep, "RECOVERY_MEMORY")["boot_count"], "32")
