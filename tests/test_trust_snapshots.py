"""Trust invariants: physical snapshot ring and destination age."""
import kmz_disk as disk
from test_integrity_gate import KmzCase
from kmz_qemu import section_kv


class T1SnapshotPlacement(KmzCase):
    def history(self):
        self.boot_once()
        template = disk.read_sector(self.img, 17)
        for boot in range(1, 17):
            s = bytearray(template); s[5] = boot; s[17] = disk.sum_byte(s[:17])
            disk.write_sector(self.img, 16 + boot % 16, bytes(s))
        n = disk.journal_used_slots(self.img)
        for i, boot in enumerate(range(2, 17)):
            disk.write_journal_slot(self.img, n+i, disk.make_entry(boot, disk.JE["CLEAN_BOOT"]))
        s = disk.resign_state(disk.read_sector(self.img, 1), boot_count=16)
        for lba in (1, 50): disk.write_sector(self.img, lba, s)

    def snapshot_bytes(self):
        return {i: disk.read_sector(self.img, i) for i in range(16, 32)}

    def test_swapped_latest_and_oldest_hold_every_sector(self):
        self.history()
        a, b = disk.read_sector(self.img, 16), disk.read_sector(self.img, 17)
        disk.write_sector(self.img, 16, b); disk.write_sector(self.img, 17, a)
        before = self.snapshot_bytes()
        report = self.boot_once()
        self.assertEqual(section_kv(report, "RECOVERY_MEMORY")["boot_count"], "17")
        self.assertEqual(self.snapshot_bytes(), before)

    def test_duplicate_valid_snapshot_cannot_authorize_rotation(self):
        self.history()
        disk.write_sector(self.img, 20, disk.read_sector(self.img, 16))
        before = self.snapshot_bytes(); self.boot_once()
        self.assertEqual(self.snapshot_bytes(), before)

    def test_correct_destination_replaces_only_aged_record(self):
        self.history(); before = self.snapshot_bytes(); self.boot_once()
        self.assertEqual(disk.read_sector(self.img, 17)[5], 17)
        for lba, raw in before.items():
            if lba != 17: self.assertEqual(disk.read_sector(self.img, lba), raw)
