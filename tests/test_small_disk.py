"""Capacity compatibility tests using disposable QEMU recovery images."""
from test_integrity_gate import KmzCase
import kmz_disk as disk
from kmz_qemu import section_kv


class D1SmallDisk(KmzCase):
    def check_capacity(self, sectors):
        self.img = disk.blank_image(self.dir / "small.img", sectors)
        disk.fill_journal(self.img, 3, boot=7)
        disk.write_legacy_state(self.img, boot_count=7, jcount=3)
        evidence = disk.journal_bytes(self.img)[:48]
        for expected_boot in (8, 9):
            rep = self.boot_once()
            store = section_kv(rep, "RECOVERY_STORE")
            self.assertEqual(store["disk_capacity_sectors"], str(sectors))
            self.assertEqual(store["state_locked"], "0")
            self.assertEqual(store["journal_locked"], "0")
            self.assertEqual(store["durable_boot_count"], str(expected_boot))
            mode = "single_copy_capacity" if sectors <= 50 else "mirror_lba50"
            self.assertEqual(store["state_redundancy"], mode)
            self.assertEqual(disk.journal_bytes(self.img)[:48], evidence)
            self.assertEqual(self.img.stat().st_size, sectors * disk.SECTOR)
            self.assertTrue(disk.state_ext_valid(disk.read_sector(self.img, 1)))
            if sectors > 50:
                self.assertEqual(disk.read_sector(self.img, 1),
                                 disk.read_sector(self.img, 50))

    def test_32_sectors(self):
        self.check_capacity(32)

    def test_50_sectors(self):
        self.check_capacity(50)

    def test_51_sectors(self):
        self.check_capacity(51)

    def test_normal_capacity(self):
        self.check_capacity(2048)
