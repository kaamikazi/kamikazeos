"""Phase A production C policy tests: no guest or intentional faults."""
import os
from pathlib import Path
import subprocess
import unittest
from test_health_policy import ROOT, build_fixture

CASES = (
    "test_valid_layout_and_exclusive_boundaries",
    "test_unaligned_empty_and_wrapped_spans_are_rejected",
    "test_gap_overlap_and_reordered_spans_are_rejected",
    "test_outside_aperture_and_wrong_fixed_sizes_are_rejected",
    "test_legacy_policy_matches_every_page_and_exactly_five_user_pages",
    "test_planned_section_permissions_do_not_imply_nx",
    "test_guard_reservations_remain_present_under_active_policy",
    "test_planned_ram_device_and_heap_availability_rules",
    "test_bad_modes_and_missing_layout_fail_closed",
)

class M1PagePolicyC(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = Path(os.environ["KMZ_TEST_RUN_DIR"]) / "memory-policy-actual"
        cls.wasm = build_fixture("memory_policy_fixture.c", cls.directory, CASES,
                                 ["kernel/src/memory_policy.c"])

    def check(self, name):
        out = subprocess.run(["node", str(ROOT / "tests/kernel_fixture.js"), str(self.wasm), name],
                             capture_output=True, text=True)
        (self.directory / (name + ".txt")).write_text(out.stdout + out.stderr)
        self.assertEqual(out.returncode, 0, out.stdout + out.stderr)

def case(name):
    def run(self): self.check(name)
    return run

for name in CASES:
    setattr(M1PagePolicyC, name, case(name))
