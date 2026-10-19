"""Phase A actual-C RAM accounting tests; no frame allocation or guest faults."""
import os
from pathlib import Path
import subprocess
import unittest
from test_health_policy import ROOT, build_fixture

CASES = (
    "test_ram_union_reserved_wins_without_double_counting",
    "test_high_ram_discovery_is_separate_from_tracking_and_mapping",
    "test_partial_adjacent_and_reserved_pages",
    "test_untrusted_fallback_never_claims_detected_or_eligible_ram",
    "test_disabled_mapping_does_not_claim_accessible_frames",
    "test_boot_reservations_round_outward_and_do_not_double_count",
    "test_inactive_heap_candidate_is_not_an_allocated_heap_reservation",
    "test_invalid_accounting_inputs_fail_closed",
    "test_small_ram_holes_and_bounded_eligibility",
)

class M2RamAccountingC(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = Path(os.environ["KMZ_TEST_RUN_DIR"]) / "memory-accounting-actual"
        cls.wasm = build_fixture("memory_accounting_fixture.c", cls.directory, CASES,
                                 ["kernel/src/memory_accounting.c", "kernel/src/memory_policy.c", "kernel/src/string.c"])

    def check(self, name):
        out = subprocess.run(["node", str(ROOT / "tests/kernel_fixture.js"), str(self.wasm), name],
                             capture_output=True, text=True)
        (self.directory / (name + ".txt")).write_text(out.stdout + out.stderr)
        self.assertEqual(out.returncode, 0, out.stdout + out.stderr)

def case(name):
    def run(self): self.check(name)
    return run

for name in CASES:
    setattr(M2RamAccountingC, name, case(name))
