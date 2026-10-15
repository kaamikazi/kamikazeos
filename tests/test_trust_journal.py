"""Producer-derived event facts, distinct from scalar field validity."""
import kmz_disk as disk
import test_trust_snapshots as snap
from test_integrity_gate import KmzCase
from kmz_qemu import section_kv

class T2EventSemantics(KmzCase):
    def test_refused_suggestion_cannot_authorize_ordering_or_deduplication(self):
        with self.boot() as q:
            q.run("approve ACT-001", r"code=REC-ACT-003")
            q.run("rollback ACT-001", r"code=REC-RB-001")
        n = disk.journal_used_slots(self.img)
        disk.write_journal_slot(self.img, n, disk.make_entry(240, 1, action=1, result=0, code="BADTUP"))
        before = disk.journal_bytes(self.img)[:16*(n+1)]
        report = self.boot_once()
        self.assertEqual(section_kv(report, "RECOVERY_MEMORY")["boot_count"], "2")
        self.assertNotIn("BADTUP", [r["code"] for r in report["JOURNAL"]])
        added = disk.journal_bytes(self.img)[16*(n+1):]
        self.assertEqual(sum(added[i:i+2] == b"\xa7\x01" and added[i+3:i+5] == b"\x01\x01"
                             for i in range(0, len(added), 16)), 1)
        self.assertEqual(disk.journal_bytes(self.img)[:len(before)], before)

    def test_boot_event_cross_product_is_not_semantic_evidence(self):
        self.boot_once(); n = disk.journal_used_slots(self.img)
        for i, (event, action, result, risk, rev) in enumerate(((8,1,1,0,1), (6,0,1,0,1), (7,4,1,0,1))):
            disk.write_journal_slot(self.img, n+i, disk.make_entry(240, event, action, result, risk, rev))
        before = disk.journal_bytes(self.img)[:16*(n+3)]
        report = self.boot_once()
        self.assertEqual(section_kv(report, "RECOVERY_MEMORY")["boot_count"], "2")
        self.assertEqual(section_kv(report, "RECOVERY_STORE")["journal_corrupt_entries"], "3")
        self.assertEqual(disk.journal_bytes(self.img)[:len(before)], before)

    def test_invalid_clean_does_not_age_the_following_destination(self):
        snap.T1SnapshotPlacement.history(self)
        n = disk.journal_used_slots(self.img)
        disk.write_journal_slot(self.img, n, disk.make_entry(17, 8, risk=2, reversible=0))
        before = {i: disk.read_sector(self.img, i) for i in range(16,32)}
        report = self.boot_once()
        # Earlier semantic rejection means real next boot 17, NOT poisoned 18.
        self.assertEqual(section_kv(report, "RECOVERY_MEMORY")["boot_count"], "17")
        self.assertEqual(disk.read_sector(self.img, 17)[5], 17)
        for lba, raw in before.items():
            if lba != 17: self.assertEqual(disk.read_sector(self.img, lba), raw)

class T3JournalChronology(KmzCase):
    def check_order(self, boots, contradictory):
        snap.T1SnapshotPlacement.history(self)
        for lba in range(2,16): disk.write_sector(self.img, lba, bytes(512))
        for i, boot in enumerate(boots):
            disk.write_journal_slot(self.img, i, disk.make_entry(boot, disk.JE["CLEAN_BOOT"]))
        before = {lba: disk.read_sector(self.img, lba) for lba in (1,50,*range(2,32))}
        report = self.boot_once()
        self.assertEqual(section_kv(report, "RECOVERY_MEMORY")["boot_count"], "33")
        for lba in range(16,32): self.assertEqual(disk.read_sector(self.img, lba), before[lba])
        if contradictory:
            for lba, raw in before.items(): self.assertEqual(disk.read_sector(self.img, lba), raw)
            self.assertEqual(section_kv(report, "RECOVERY_STORE")["journal_locked"], "1")

    def test_reversed_append_stream_holds_persistence(self): self.check_order(range(32,0,-1), True)
    def test_duplicate_completion_holds_persistence(self): self.check_order([*range(1,25),24,*range(25,33)], True)
    def test_missing_completion_does_not_prove_snapshot_age(self): self.check_order([b for b in range(1,33) if b != 24], False)
