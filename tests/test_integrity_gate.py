"""KamikazeOS v2.6.1 "Integrity Gate" regression suite.

Every test boots the real kernel headlessly in QEMU against a DISPOSABLE
disk image created inside the run directory (never build/recovery.img).
Failure injection (torn writes, bad checksums, full journals, corrupt file
data) is done by editing those disposable images between power-ons.

Environment (set by tests/run_regression.py):
  KMZ_TEST_ELF           kernel under test
  KMZ_TEST_RUN_DIR       where per-test images and serial logs are kept
  KMZ_BASELINE_ELF       optional pre-2.6.1 kernel for compatibility tests
"""

from __future__ import annotations

import json
import os
import re
import subprocess
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import kmz_disk as disk  # noqa: E402
from kmz_qemu import QemuSession, events, parse_last_report, section_kv  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
ELF = Path(os.environ.get("KMZ_TEST_ELF", ROOT / "build" / "kamikaze.elf"))
RUN_DIR = Path(os.environ.get("KMZ_TEST_RUN_DIR", ROOT / "build" / "test-runs" / "adhoc"))
BASELINE_ELF = os.environ.get("KMZ_BASELINE_ELF")

PKG_ACT_OR_BLOCK = r"code=(PKG-ACT-00\d|PKG-BLOCK-001|PKG-REJECT-\d+)[^\n]*"
RB_RESULT = r"code=(PKG-RB-001|PKG-REJECT-\d+)[^\n]*"


class KmzCase(unittest.TestCase):
    """Per-test disposable image + serial log, plus report helpers."""

    def setUp(self) -> None:
        self.dir = RUN_DIR / self.id().split(".")[-1]
        self.dir.mkdir(parents=True, exist_ok=True)
        self.img = disk.blank_image(self.dir / "disk.img")
        self.log = self.dir / "serial.log"
        self.log.unlink(missing_ok=True)

    def boot(self, elf: Path | None = None) -> QemuSession:
        return QemuSession(elf or ELF, self.img, self.log)

    def report(self, q: QemuSession) -> dict:
        return q.export_report()

    @staticmethod
    def health(report: dict) -> int:
        return int(section_kv(report, "CURRENT_HEALTH")["score"])

    @staticmethod
    def rows(report: dict, section: str, key: str = "id") -> dict:
        return {r[key]: r for r in report.get(section, []) if key in r}

    def boot_once(self, commands: list[tuple[str, str | None]] = ()) -> dict:
        with self.boot() as q:
            for cmd, expect in commands:
                q.run(cmd, expect)
            return self.report(q)


# ===================================================================== F1
class F1JournalIntegrity(KmzCase):
    """Journal evidence must never be overwritten after state damage."""

    def test_torn_append_entry_is_not_overwritten(self):
        self.boot_once()
        used = disk.journal_used_slots(self.img)
        self.assertGreater(used, 0)
        # Torn append: the entry reached the journal sector but the state
        # block's count update never landed.
        torn = disk.make_entry(77, disk.JE["APPROVED"], action=1, code="TORN01")
        disk.write_journal_slot(self.img, used, torn)
        self.boot_once()
        self.assertEqual(disk.journal_slot(self.img, used), torn,
                         "torn-append evidence was overwritten")
        self.assertGreater(disk.journal_used_slots(self.img), used + 1,
                           "new entries must land after the torn entry")

    def test_bad_primary_state_checksum_keeps_history_and_boot_count(self):
        self.boot_once()
        self.boot_once()
        before = disk.journal_bytes(self.img)
        used = disk.journal_used_slots(self.img)
        boots = disk.state_fields(self.img)["boot_count"]
        disk.corrupt_byte(self.img, disk.LBA_STATE, 5)   # boot count byte
        rep = self.boot_once()
        after = disk.journal_bytes(self.img)
        self.assertEqual(after[:used * disk.ENTRY_SIZE],
                         before[:used * disk.ENTRY_SIZE],
                         "existing journal history was overwritten")
        self.assertGreater(disk.journal_used_slots(self.img), used)
        store = section_kv(rep, "RECOVERY_STORE")
        self.assertEqual(int(store["durable_boot_count"]), boots + 1,
                         "redundant state should carry the boot count across "
                         "a damaged primary state sector")

    def test_both_state_copies_corrupt_history_preserved(self):
        self.boot_once()
        self.boot_once()
        before = disk.journal_bytes(self.img)
        used = disk.journal_used_slots(self.img)
        disk.corrupt_byte(self.img, disk.LBA_STATE, 5)
        disk.corrupt_byte(self.img, disk.LBA_STATE_MIRROR, 5)
        with self.boot() as q:
            text = q.boot_text()
            rep = self.report(q)
        self.assertEqual(disk.journal_bytes(self.img)[:used * disk.ENTRY_SIZE],
                         before[:used * disk.ENTRY_SIZE])
        self.assertGreaterEqual(int(section_kv(rep, "RECOVERY_STORE")["journal_count"]), used)
        self.assertTrue(events(text, "STORE-WARN-008") or events(text, "STORE-ERR-010"),
                        "reconstruction from journal evidence must be logged loudly")

    def test_stored_count_ahead_of_entries_never_reuses_claimed_slots(self):
        disk.fill_journal(self.img, 5)
        disk.write_legacy_state(self.img, boot_count=3, jcount=10)
        self.boot_once()
        for i in range(5, 10):
            self.assertEqual(disk.journal_slot(self.img, i), bytes(16),
                             f"slot {i} claimed by state must stay untouched")
        self.assertGreater(disk.journal_used_slots(self.img), 10)

    def test_corrupt_middle_entry_is_preserved(self):
        disk.fill_journal(self.img, 6)
        disk.write_legacy_state(self.img, boot_count=3, jcount=6)
        disk.corrupt_byte(self.img, disk.LBA_JOURNAL, 2 * 16 + 9)  # entry 2 csum
        disk.corrupt_byte(self.img, disk.LBA_STATE, 5)             # and state
        damaged = disk.journal_bytes(self.img)[:6 * 16]
        self.boot_once()
        self.assertEqual(disk.journal_bytes(self.img)[:6 * 16], damaged)
        self.assertGreater(disk.journal_used_slots(self.img), 6)

    def test_full_journal_is_never_rewritten_and_warns_once(self):
        disk.fill_journal(self.img, disk.JOURNAL_CAPACITY)
        disk.write_legacy_state(self.img, boot_count=9, jcount=disk.JOURNAL_CAPACITY)
        before = disk.journal_bytes(self.img)
        for _ in range(2):
            with self.boot() as q:
                # Journals a BLOCKED record on every attempt, every boot.
                q.run("approve ACT-999", r"code=REC-BLOCK-999")
                q.run("approve ACT-999", r"code=REC-BLOCK-999")
                rep = self.report(q)
                text = q.boot_text()
            self.assertEqual(disk.journal_bytes(self.img), before,
                             "full journal history changed")
            self.assertEqual(len(events(text, "STORE-WARN-004")), 1,
                             "full journal must warn exactly once per boot")
            self.assertEqual(int(section_kv(rep, "RECOVERY_STORE")["journal_count"]),
                             disk.JOURNAL_CAPACITY)

    def test_ambiguous_equal_generation_copies_lock_journal(self):
        self.boot_once()
        primary = disk.read_sector(self.img, disk.LBA_STATE)
        mirror = disk.read_sector(self.img, disk.LBA_STATE_MIRROR)
        self.assertTrue(disk.state_ext_valid(primary) and disk.state_ext_valid(mirror),
                        "kernel must write generation-aware state to both copies")
        # Same generation, different content, both internally valid:
        # nothing on disk says which one is true.
        disk.write_sector(self.img, disk.LBA_STATE_MIRROR,
                          disk.resign_state(mirror, boot_count=200))
        journal = disk.journal_bytes(self.img)
        p_before = disk.read_sector(self.img, disk.LBA_STATE)
        m_before = disk.read_sector(self.img, disk.LBA_STATE_MIRROR)
        with self.boot() as q:
            q.run("approve ACT-001", r"code=REC-ACT-003")
            q.run("approve ACT-999", r"code=REC-BLOCK-999")
            text = q.boot_text()
        self.assertTrue(events(text, "STORE-ERR-011"), "ambiguity must be reported")
        self.assertEqual(len(events(text, "STORE-WARN-013")), 1)
        self.assertEqual(disk.journal_bytes(self.img), journal,
                         "journal must be read-only while state is ambiguous")
        self.assertEqual(disk.read_sector(self.img, disk.LBA_STATE), p_before)
        self.assertEqual(disk.read_sector(self.img, disk.LBA_STATE_MIRROR), m_before)

    def test_legacy_image_upgrades_in_place(self):
        disk.fill_journal(self.img, 3, boot=4)
        disk.write_legacy_state(self.img, boot_count=4, jcount=3)
        rep = self.boot_once()
        st = disk.state_fields(self.img)
        self.assertTrue(st["magic_ok"] and st["version"] == 1 and st["v1_checksum_ok"],
                        "LBA 1 must stay readable by pre-2.6.1 kernels")
        self.assertEqual(st["boot_count"], 5)
        self.assertEqual(int(section_kv(rep, "RECOVERY_STORE")["durable_boot_count"]), 5)
        self.assertEqual(disk.journal_bytes(self.img)[:48],
                         b"".join(disk.make_entry(4, disk.JE["CLEAN_BOOT"], code="FILLER")
                                  for _ in range(3)))

    @unittest.skipUnless(BASELINE_ELF, "KMZ_BASELINE_ELF not set")
    def test_downgrade_old_kernel_reads_new_image(self):
        self.boot_once()
        self.boot_once()
        with self.boot(Path(BASELINE_ELF)) as q:
            rep = self.report(q)
        store = section_kv(rep, "RECOVERY_STORE")
        self.assertEqual(store["state_valid"], "1")
        self.assertEqual(int(store["durable_boot_count"]), 3)
        # ...and the new kernel accepts the image the old kernel last wrote.
        rep = self.boot_once()
        self.assertEqual(int(section_kv(rep, "RECOVERY_STORE")["durable_boot_count"]), 4)


# ===================================================================== F2
def _write_chain(img: Path) -> None:
    disk.write_manifest(img, 0, disk.fixture_manifest_sector(
        "PKG-201", "ga_policy_step_one", "Fixture GhostAttach 0.3 to 0.4",
        "GhostAttach", "0.3", "0.4"))
    disk.write_manifest(img, 1, disk.fixture_manifest_sector(
        "PKG-202", "ga_policy_step_two", "Fixture GhostAttach 0.4 to 0.5",
        "GhostAttach", "0.4", "0.5"))


class F2PackageRollback(KmzCase):

    def slots(self, rep):
        return self.rows(rep, "MODULE_SLOTS", "module")["GhostAttach"]

    def test_sequential_updates_reject_stale_rollback(self):
        _write_chain(self.img)
        with self.boot() as q:
            for verb in ("approve-pkg", "apply-pkg", "verify-pkg"):
                self.assertIn("PKG-ACT", q.run(f"{verb} PKG-201", PKG_ACT_OR_BLOCK))
            for verb in ("approve-pkg", "apply-pkg"):
                self.assertIn("PKG-ACT", q.run(f"{verb} PKG-202", PKG_ACT_OR_BLOCK))
            before = self.slots(self.report(q))
            self.assertEqual(before["slot_a" if before["active"] == "A" else "slot_b"], "0.5")

            # PKG-201 no longer owns the active slot: rollback must refuse.
            res = q.run("rollback-pkg PKG-201", RB_RESULT)
            self.assertIn("PKG-REJECT", res, "stale rollback of PKG-201 was accepted")
            rep = self.report(q)
            self.assertEqual(self.slots(rep), before, "slots changed by refused rollback")
            self.assertEqual(self.rows(rep, "PACKAGE_STATES")["PKG-201"]["rolled_back"], "0")

            # The owner can roll back; GhostAttach returns to 0.4.
            self.assertIn("PKG-RB-001", q.run("rollback-pkg PKG-202", RB_RESULT))
            after = self.slots(self.report(q))
            self.assertEqual(after["slot_a" if after["active"] == "A" else "slot_b"], "0.4")

            # PKG-201's from-version slot was consumed by PKG-202's apply:
            # there is no known-good 0.3 to return to, so refuse.
            self.assertIn("PKG-REJECT", q.run("rollback-pkg PKG-201", RB_RESULT))
            self.assertEqual(self.slots(self.report(q)), after)

    def test_apply_refused_while_previous_update_unverified(self):
        _write_chain(self.img)
        with self.boot() as q:
            q.run("approve-pkg PKG-201", PKG_ACT_OR_BLOCK)
            q.run("apply-pkg PKG-201", PKG_ACT_OR_BLOCK)
            q.run("approve-pkg PKG-202", PKG_ACT_OR_BLOCK)
            before = self.slots(self.report(q))
            res = q.run("apply-pkg PKG-202", PKG_ACT_OR_BLOCK)
            self.assertIn("PKG-REJECT", res,
                          "apply over an unverified update destroys the only verified slot")
            self.assertEqual(self.slots(self.report(q)), before)

    def test_rollback_survives_restart_ownership_check(self):
        _write_chain(self.img)
        with self.boot() as q:
            for verb in ("approve-pkg", "apply-pkg", "verify-pkg"):
                q.run(f"{verb} PKG-201", PKG_ACT_OR_BLOCK)
        with self.boot() as q:
            self.assertIn("PKG-RB-001", q.run("rollback-pkg PKG-201", RB_RESULT))
            s = self.slots(self.report(q))
            self.assertEqual(s["slot_a" if s["active"] == "A" else "slot_b"], "0.3")


# ===================================================================== F3
class F3ManifestSeeding(KmzCase):

    def test_seeded_builtin_packages_stay_usable(self):
        with self.boot() as q:
            q.run("seed-manifests confirm", r"code=MAN-002")
            self.assertIn("PKG-ACT-001", q.run("approve-pkg PKG-001", PKG_ACT_OR_BLOCK))
        rep = self.boot_once()
        pkgs = self.rows(rep, "RECOVERY_PACKAGES")
        self.assertEqual(pkgs["PKG-001"]["status"], "approved")
        self.assertEqual(pkgs["PKG-002"]["status"], "suggested")
        self.assertEqual(pkgs["PKG-003"]["status"], "blocked")
        for pid in ("PKG-001", "PKG-002", "PKG-004"):
            mode = pkgs[pid]["signature_mode"]
            self.assertNotIn("ed25519", mode, f"{pid} must not claim Ed25519 trust")
            self.assertNotIn("trusted", mode)

    def test_tampered_seed_on_disk_is_blocked(self):
        from manifest_lib import Manifest, manifest_to_sector
        m = Manifest.from_json({
            "package_id": "PKG-001", "name": "ghostattach_policy_update",
            "description": "Update GhostAttach attach-policy 0.3 to 0.9",
            "target_component": "GhostAttach", "target_module": "GhostAttach",
            "from_version": "0.3", "to_version": "0.4", "risk": "low",
            "reversible": True, "requires_approval": True,
            "signature_status": "signed_placeholder", "trust_anchor_id": 1,
            "manifest_version": 1, "payload_type": "module_slot_switch_simulated"})
        disk.write_manifest(self.img, 0, manifest_to_sector(m))
        pkgs = self.rows(self.boot_once(), "RECOVERY_PACKAGES")
        self.assertEqual(pkgs["PKG-001"]["status"], "blocked")

    def test_seeding_never_overwrites_imported_manifest(self):
        from manifest_lib import load_manifest, manifest_to_sector
        sec = manifest_to_sector(load_manifest(
            ROOT / "packages" / "examples" / "PKG-101-ghostattach-policy.json"))
        disk.write_manifest(self.img, 0, sec)
        with self.boot() as q:
            q.run("seed-manifests confirm", r"code=MAN-00[23]")
        self.assertEqual(disk.read_sector(self.img, disk.LBA_MANIFESTS), sec)
        pkgs = self.rows(self.boot_once(), "RECOVERY_PACKAGES")
        self.assertIn("PKG-101", pkgs)
        self.assertEqual(pkgs["PKG-101"]["status"], "suggested")
        self.assertEqual(pkgs["PKG-001"]["status"], "suggested")

    def test_real_signed_import_still_allowed(self):
        from manifest_lib import load_manifest, manifest_to_sector
        disk.write_manifest(self.img, 0, manifest_to_sector(load_manifest(
            ROOT / "packages" / "examples" / "PKG-101-ghostattach-policy.json")))
        pkgs = self.rows(self.boot_once(), "RECOVERY_PACKAGES")
        self.assertEqual(pkgs["PKG-101"]["policy_result"], "allowed")

    def test_marker_with_foreign_key_hash_is_blocked(self):
        disk.write_manifest(self.img, 0, disk.fixture_manifest_sector(
            "PKG-203", "foreign_key_fixture", "Fixture signed by unknown key",
            "GhostAttach", "0.3", "0.4", key_hash_prefix="deadbeef" * 8))
        pkgs = self.rows(self.boot_once(), "RECOVERY_PACKAGES")
        self.assertEqual(pkgs["PKG-203"]["policy_result"], "blocked")


# ===================================================================== F4
class F4HealthScore(KmzCase):

    def test_clean_boot_health_counts_only_device_state(self):
        rep = self.boot_once()
        meta = {k: int(v) for r in rep["META"] for k, v in r.items()
                if k.startswith("scan_")}
        production_isolated = meta["scan_isolated"] - meta["scan_expected_self_tests"]
        expected = 100 - 10 * meta["scan_missing"] - 8 * production_isolated
        self.assertEqual(self.health(rep), expected,
                         "expected canaries must not cost production health")

    def test_shell_typos_do_not_change_health(self):
        with self.boot() as q:
            h0 = self.health(self.report(q))
            for i in range(5):
                q.run(f"typo{i}", r"code=SHL-001")
            h1 = self.health(self.report(q))
        self.assertEqual(h1, h0)

    def test_real_fault_counted_and_ring_wrap_cannot_improve_health(self):
        with self.boot() as q:
            q.run("fs-format confirm", r"code=TFS-FMT-000")
            q.run("touch f1", r"code=TFS-CREATE-000")
            q.run("write f1 hello world", r"code=TFS-WRITE-000")
        disk.corrupt_byte(self.img, 73, 0)        # first TinyFS data sector
        with self.boot() as q:
            h0 = self.health(self.report(q))
            q.run("cat f1", r"code=TFS-ERR-014")
            h1 = self.health(self.report(q))
            self.assertEqual(h1, max(0, h0 - 25), "a real data-integrity fault must cost health")
            for _ in range(36):                  # 72 info events > 64-entry ring
                q.run("run hello", r"code=APP-RUN-001")
            h2 = self.health(self.report(q))
        self.assertEqual(h2, h1, "log-ring wraparound must not improve health")

    def test_verify_not_failed_by_unrelated_typos(self):
        with self.boot() as q:
            q.run("approve-pkg PKG-001", PKG_ACT_OR_BLOCK)
            q.run("apply-pkg PKG-001", PKG_ACT_OR_BLOCK)
            for i in range(6):
                q.run(f"oops{i}", r"code=SHL-001")
            res = q.run("verify-pkg PKG-001", PKG_ACT_OR_BLOCK)
        self.assertIn("PKG-ACT-003", res)

    def test_intentional_panic_test_not_penalized_next_boot(self):
        with self.boot() as q:
            q.run("panic-test", r"code=TEST-PANIC-001")
        rep = self.boot_once()
        h = section_kv(rep, "CURRENT_HEALTH")
        self.assertEqual(section_kv(rep, "RECOVERY_MEMORY")["last_boot_panic"], "1",
                         "panic evidence must still be reported")
        self.assertEqual(h["panic_penalty"], "0")

    def test_unclassified_previous_panic_still_penalized(self):
        disk.write_legacy_state(self.img, boot_count=2, jcount=0, panic=1)
        h = section_kv(self.boot_once(), "CURRENT_HEALTH")
        self.assertEqual(h["panic_penalty"], "15")


# ===================================================================== F5
class F5JournalLongevity(KmzCase):

    def test_unchanged_suggestion_not_rejournaled_each_boot(self):
        for _ in range(3):
            self.boot_once()
        suggested = [disk.decode_entry(e) for e in disk.journal_entries(self.img)]
        act1 = [e for e in suggested if e["event"] == "SUGGESTED" and e["action"] == 1]
        self.assertEqual(len(act1), 1, f"ACT-001 suggestion journaled {len(act1)} times")

    def test_steady_state_journal_growth_is_one_entry_per_boot(self):
        self.boot_once()
        self.boot_once()
        before = disk.journal_used_slots(self.img)
        self.boot_once()
        self.boot_once()
        grown = disk.journal_used_slots(self.img) - before
        events_added = [disk.decode_entry(e)["event"]
                        for e in disk.journal_entries(self.img)[before:]]
        self.assertEqual(events_added, ["CLEAN_BOOT", "CLEAN_BOOT"],
                         f"unchanged boots appended {grown} entries: {events_added}")

    def test_suggestion_rejournaled_after_state_change(self):
        with self.boot() as q:
            q.run("approve ACT-001", r"code=REC-ACT-003")
            q.run("rollback ACT-001", r"code=REC-RB-001")
        self.boot_once()
        entries = [disk.decode_entry(e) for e in disk.journal_entries(self.img)]
        act1 = [e["event"] for e in entries if e["action"] == 1 and
                e["event"] in ("SUGGESTED", "APPROVED", "APPLIED", "ROLLED_BACK")]
        self.assertEqual(act1, ["SUGGESTED", "APPROVED", "APPLIED", "ROLLED_BACK",
                                "SUGGESTED"])

    def test_full_journal_costs_health_once(self):
        fresh = self.health(self.boot_once())
        self.img = disk.blank_image(self.dir / "full.img")
        disk.fill_journal(self.img, disk.JOURNAL_CAPACITY)
        disk.write_legacy_state(self.img, boot_count=1, jcount=disk.JOURNAL_CAPACITY)
        with self.boot() as q:
            q.run("approve ACT-001", r"code=REC-ACT-003")
            q.run("rollback ACT-001", r"code=REC-RB-001")
            rep = self.report(q)
        store = section_kv(rep, "RECOVERY_STORE")
        self.assertEqual(store.get("journal_full"), "1")
        self.assertGreaterEqual(int(store.get("journal_dropped", "0")), 3)
        self.assertEqual(self.health(rep), fresh - 5)


# ================================================================ regression
class R1Regression(KmzCase):

    def test_ring3_held_and_privilege_self_tests_pass(self):
        with self.boot() as q:
            text = q.boot_text()
            res = q.run("ring3-test", r"code=RING3-10\d[^\n]*")
            rep = self.report(q)
        self.assertTrue(events(text, "RING3-HOLD"))
        self.assertFalse(events(text, "RING3-READY"))
        self.assertIn("RING3-102", res)
        ring3 = section_kv(rep, "RING3")
        self.assertEqual(ring3["execution_approved"], "no")
        self.assertEqual(ring3["execution_enabled"], "no")
        self.assertEqual(ring3["test_status"], "never_run")
        sys_ = section_kv(rep, "SYSCALLS")
        self.assertEqual(sys_["origin_policy_self_test"], "pass")
        self.assertEqual(sys_["register_preservation_test"], "pass")
        for bad in ("INT-102", "SYS-ERR-005", "PAGE-101", "PAGE-102", "TSS-101",
                    "TSS-102", "RING3-101", "MEM-102"):
            self.assertFalse(events(text, bad), f"{bad} logged at boot")
        self.assertEqual(section_kv(rep, "PAGING")["enabled"], "yes")

    def test_tinyfs_roundtrip_across_restart(self):
        with self.boot() as q:
            q.run("fs-format confirm", r"code=TFS-FMT-000")
            q.run("touch notes.txt", r"code=TFS-CREATE-000")
            q.run("write notes.txt integrity gate", r"code=TFS-WRITE-000")
        with self.boot() as q:
            q.run("cat notes.txt", r"code=TFS-READ-000")
            files = self.rows(self.report(q), "TINYFS_FILES", "name")
            self.assertEqual(files["notes.txt"]["size"], "14")
            q.run("rm notes.txt", r"code=TFS-DELETE-000")
        rep = self.boot_once()
        self.assertNotIn("notes.txt", self.rows(rep, "TINYFS_FILES", "name"))

    def test_recovery_action_persists_and_rolls_back(self):
        with self.boot() as q:
            q.run("approve ACT-001", r"code=REC-ACT-003")
        with self.boot() as q:
            self.assertTrue(events(q.boot_text(), "REC-MEM-001"))
            q.run("rollback ACT-001", r"code=REC-RB-001")
        rep = self.boot_once()
        self.assertEqual(section_kv(rep, "RECOVERY_MEMORY")["network_quarantined"], "0")

    def test_canaries_still_blocked(self):
        with self.boot() as q:
            q.run("approve ACT-999", r"code=REC-BLOCK-999")
            res = q.run("approve-pkg PKG-003", PKG_ACT_OR_BLOCK)
            rep = self.report(q)
        self.assertIn("PKG-BLOCK-001", res)
        self.assertEqual(self.rows(rep, "RECOVERY_PACKAGES")["PKG-003"]["status"], "blocked")
        actions = self.rows(rep, "RECOVERY_ACTIONS")
        self.assertEqual(actions["ACT-999"]["status"], "blocked")

    def test_analyzer_and_dashboard_consume_report(self):
        with self.boot() as q:
            q.run("approve ACT-001", r"code=REC-ACT-003")
            q.run("typo", r"code=SHL-001")
            self.report(q)
        out = subprocess.run([sys.executable, str(ROOT / "ai-recovery" / "analyze_logs.py"),
                              str(self.log)], capture_output=True, text=True)
        (self.dir / "analyzer.txt").write_text(out.stdout + out.stderr)
        self.assertIn(out.returncode, (0, 1), out.stderr)
        self.assertNotIn("Traceback", out.stderr)
        self.assertIn("RECOVERY CORE", out.stdout)
        dash = self.dir / "dashboard.json"
        out = subprocess.run([sys.executable,
                              str(ROOT / "ai-recovery" / "export_dashboard_json.py"),
                              str(self.log), str(dash)], capture_output=True, text=True)
        self.assertEqual(out.returncode, 0, out.stdout + out.stderr)
        data = json.loads(dash.read_text())
        for key in ("health", "devices", "packages"):
            self.assertIn(key, data)
        js = subprocess.run(["node", str(ROOT / "tests" / "dashboard_smoke.js"),
                             str(dash)], capture_output=True, text=True)
        self.assertEqual(js.returncode, 0, js.stdout + js.stderr)


if __name__ == "__main__":
    unittest.main()
