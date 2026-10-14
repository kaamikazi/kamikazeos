"""Takeover regressions: small disks, snapshot evidence, legacy generations.

All modified media belong to KmzCase's disposable run directory. Read errors
are injected by QEMU blkdebug; the original recovery image is never opened.
"""
from test_integrity_gate import KmzCase
import kmz_disk as disk
from kmz_qemu import QemuSession, events, section_kv


def snapshot_bytes(img):
    return b"".join(disk.read_sector(img, lba) for lba in range(16, 32))


class D2SnapshotEvidence(KmzCase):
    def prepare_history(self):
        self.boot_once()
        self.boot_once()

    def check_locked(self, read_errors=(), conflict=False):
        self.prepare_history()
        if conflict:
            mirror = disk.read_sector(self.img, 50)
            disk.write_sector(self.img, 50, disk.resign_state(mirror, boot_count=200))
        snaps = snapshot_bytes(self.img)
        journal = disk.journal_bytes(self.img)
        primary = disk.read_sector(self.img, 1)
        mirror = disk.read_sector(self.img, 50)
        with QemuSession(self.boot().elf, self.img, self.log,
                         read_error_lbas=read_errors) as q:
            rep = self.report(q)
            text = q.boot_text()
        store = section_kv(rep, "RECOVERY_STORE")
        self.assertEqual(snapshot_bytes(self.img), snaps,
                         "locked storage rewrote historical snapshot evidence")
        self.assertEqual(disk.journal_bytes(self.img), journal)
        self.assertEqual(disk.read_sector(self.img, 1), primary)
        self.assertEqual(disk.read_sector(self.img, 50), mirror)
        self.assertEqual(store["journal_locked"], "1")
        self.assertTrue(events(text, "SNAP-WARN-002"))
        self.assertGreaterEqual(int(section_kv(rep, "RECOVERY_MEMORY")["boot_count"]), 3)

    def test_conflicting_state_preserves_snapshots(self):
        self.check_locked(conflict=True)

    def test_unreadable_primary_preserves_snapshots(self):
        self.check_locked(read_errors=(1,))

    def test_unreadable_mirror_preserves_snapshots(self):
        self.check_locked(read_errors=(50,))

    def test_unreadable_journal_preserves_snapshots(self):
        self.check_locked(read_errors=(2,))

    def test_both_corrupt_reconstructs_without_reusing_snapshots(self):
        self.prepare_history()
        old_slots = {lba: disk.read_sector(self.img, lba) for lba in (17, 18)}
        disk.corrupt_byte(self.img, 1, 5)
        disk.corrupt_byte(self.img, 50, 5)
        rep = self.boot_once()
        self.assertEqual(section_kv(rep, "RECOVERY_MEMORY")["boot_count"], "3")
        for lba, before in old_slots.items():
            self.assertEqual(disk.read_sector(self.img, lba), before)

    def test_snapshot_ahead_of_reconstructed_boot_holds_writes(self):
        self.prepare_history()
        sec = bytearray(disk.read_sector(self.img, 18))
        sec[5] = 70
        sec[17] = disk.sum_byte(sec[:17])
        disk.write_sector(self.img, 22, bytes(sec))
        disk.corrupt_byte(self.img, 1, 5)
        disk.corrupt_byte(self.img, 50, 5)
        before = snapshot_bytes(self.img)
        with self.boot() as q:
            self.report(q)
            self.assertTrue(events(q.boot_text(), "SNAP-WARN-002"))
        self.assertEqual(snapshot_bytes(self.img), before)

    def test_exhausted_boot_counter_preserves_evidence(self):
        self.prepare_history()
        disk.write_legacy_state(self.img, boot_count=255,
                                jcount=disk.journal_used_slots(self.img))
        snaps, journal = snapshot_bytes(self.img), disk.journal_bytes(self.img)
        primary, mirror = disk.read_sector(self.img, 1), disk.read_sector(self.img, 50)
        with self.boot() as q:
            rep = self.report(q)
            self.assertTrue(events(q.boot_text(), "STORE-ERR-015"))
        self.assertEqual(snapshot_bytes(self.img), snaps)
        self.assertEqual(disk.journal_bytes(self.img), journal)
        self.assertEqual(disk.read_sector(self.img, 1), primary)
        self.assertEqual(disk.read_sector(self.img, 50), mirror)
        self.assertEqual(section_kv(rep, "RECOVERY_STORE")["boot_order_trusted"], "0")


