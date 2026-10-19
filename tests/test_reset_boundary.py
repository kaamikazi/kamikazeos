"""Exact-factory reset image and one-shot reset grant, in the actual C writers."""
import os
from pathlib import Path
import subprocess
import unittest

from test_health_policy import ROOT, build_fixture

CASES = (
    # F1: exact canonical factory image at the reset exception
    "test_reset_rejects_duplicate_module_identity",
    "test_reset_rejects_reordered_module_identities",
    "test_reset_rejects_inactive_version_tail_byte",
    "test_reset_rejects_active_version_tail_byte",
    "test_reset_rejects_changed_flags_and_foreign_identity",
    "test_reset_accepts_exact_factory_image",
    "test_legitimate_reset_workflow_writes_canonical_image",
    "test_factory_image_is_not_an_ordinary_update_exception",
    # F2: one-shot reset authorization
    "test_rejected_reset_slot_proposal_revokes_grant",
    "test_rejected_reset_state_proposal_revokes_grant",
    "test_out_of_order_reset_slot_write_revokes_grant",
    "test_reset_io_failures_revoke_grant_and_hold",
    "test_successful_reset_consumes_grant",
)


class T6ResetBoundaryC(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = Path(os.environ["KMZ_TEST_RUN_DIR"]) / "reset-boundary-actual"
        cls.wasm = build_fixture("reset_boundary_fixture.c", cls.directory, CASES,
                                 ["kernel/src/string.c", "kernel/src/package_state.c",
                                  "kernel/src/module_slots.c", "kernel/src/module_versions.c"])

    def check(self, name):
        out = subprocess.run(["node", str(ROOT / "tests/kernel_fixture.js"),
                              str(self.wasm), name], capture_output=True, text=True)
        (self.directory / (name + ".txt")).write_text(out.stdout + out.stderr)
        self.assertEqual(out.returncode, 0, out.stdout + out.stderr)


def case(name):
    def run(self):
        self.check(name)
    return run


for _name in CASES:
    setattr(T6ResetBoundaryC, _name, case(_name))
