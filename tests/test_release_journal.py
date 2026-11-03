"""Release-blocker journal semantics and verified snapshot progression."""
import kmz_disk as disk
from kmz_qemu import section_kv
from test_integrity_gate import KmzCase


def snapshots(img):
    return {lba: disk.read_sector(img, lba) for lba in range(16, 32)}


class R5JournalEvidence(KmzCase):
    def malformed_field(self, field):
        with self.boot() as q:
            q.run("approve ACT-001", r"code=REC-ACT-003")
            q.run("rollback ACT-001", r"code=REC-RB-001")
        n = disk.journal_used_slots(self.img)
        bad = bytearray(disk.make_entry(240, disk.JE["SUGGESTED"], action=1, code="BADSEM"))
        bad[field] = 255
        bad[15] = disk.sum_byte(bad[:15])
        disk.write_journal_slot(self.img, n, bytes(bad))
        prefix = disk.journal_bytes(self.img)[:(n + 1) * 16]
        history = {lba: raw for lba, raw in snapshots(self.img).items() if any(raw)}
        report = self.boot_once()
        with self.subTest(consumer="ordering"):
            self.assertEqual(section_kv(report, "RECOVERY_MEMORY")["boot_count"], "2")
        with self.subTest(consumer="export"):
            self.assertNotIn("BADSEM", [r["code"] for r in report["JOURNAL"]])
        with self.subTest(consumer="suggestion"):
            tail = disk.journal_bytes(self.img)[(n + 1) * 16:]
            self.assertEqual(sum(tail[i:i+2] == b"\xa7\x01" and
                                 tail[i+3:i+5] == b"\x01\x01"
                                 for i in range(0, len(tail), 16)), 1)
        with self.subTest(consumer="snapshot_history"):
            for lba, raw in history.items():
                self.assertEqual(disk.read_sector(self.img, lba), raw)
        self.assertEqual(disk.journal_bytes(self.img)[:len(prefix)], prefix)
        self.assertEqual(section_kv(report, "RECOVERY_STORE")["journal_corrupt_entries"], "1")

    def test_invalid_result_boot_240_is_not_evidence(self): self.malformed_field(5)
    def test_invalid_risk_boot_240_is_not_evidence(self): self.malformed_field(6)
    def test_invalid_reversible_boot_240_is_not_evidence(self): self.malformed_field(7)

    def test_reserved_action_and_short_code_domains(self):
        self.boot_once()
        n = disk.journal_used_slots(self.img)
        mutations = ((14, 1), (4, 5), (8, 10), (8, 127), (9, 0))
        for i, (field, value) in enumerate(mutations):
            bad = bytearray(disk.make_entry(240, disk.JE["SUGGESTED"], action=1, code="BADTAG"))
            bad[field] = value
            bad[15] = disk.sum_byte(bad[:15])
            disk.write_journal_slot(self.img, n + i, bytes(bad))
        prefix = disk.journal_bytes(self.img)[:(n + len(mutations)) * 16]
        report = self.boot_once()
        self.assertEqual(section_kv(report, "RECOVERY_MEMORY")["boot_count"], "2")
        self.assertEqual(section_kv(report, "RECOVERY_STORE")["journal_corrupt_entries"], "5")
        self.assertEqual(disk.journal_bytes(self.img)[:len(prefix)], prefix)

    def test_valid_but_uncorroborated_boot_jump_cannot_replace_snapshots(self):
        self.boot_once()
        n = disk.journal_used_slots(self.img)
        disk.write_journal_slot(self.img, n, disk.make_entry(
            240, disk.JE["SUGGESTED"], action=1, code="JUMP01"))
        before = snapshots(self.img)
        report = self.boot_once()
        self.assertEqual(section_kv(report, "RECOVERY_MEMORY")["boot_count"], "241")
        self.assertEqual(snapshots(self.img), before,
                         "a valid record is not proof of all intervening boots")

    def test_complete_clean_boot_chain_allows_verified_snapshot_gap(self):
        self.boot_once()
        template = disk.read_sector(self.img, 17)
        for boot in range(1, 17):
            raw = bytearray(template)
            raw[5] = boot
            raw[17] = disk.sum_byte(raw[:17])
            disk.write_sector(self.img, 16 + boot % 16, bytes(raw))
        n = disk.journal_used_slots(self.img)
        for i, boot in enumerate(range(2, 34)):
            disk.write_journal_slot(self.img, n + i,
                                    disk.make_entry(boot, disk.JE["CLEAN_BOOT"]))
        before = snapshots(self.img)
        report = self.boot_once()
        self.assertEqual(section_kv(report, "RECOVERY_MEMORY")["boot_count"], "34")
        self.assertEqual(disk.read_sector(self.img, 18)[5], 34)
        for lba, raw in before.items():
            if lba != 18:
                self.assertEqual(disk.read_sector(self.img, lba), raw)

    def test_legitimate_snapshot_ring_rotates_after_18_boots(self):
        for _ in range(18):
            self.boot_once()
        self.assertEqual(sorted(raw[5] for raw in snapshots(self.img).values()),
                         list(range(3, 19)))
