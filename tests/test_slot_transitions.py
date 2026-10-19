"""Per-module durable-to-proposed authorization in the actual C slot writer."""
import os
from pathlib import Path
import subprocess
import unittest

from test_health_policy import ROOT, build_fixture


class T5SlotTransitionsC(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = Path(os.environ["KMZ_TEST_RUN_DIR"]) / "slot-transitions-actual"
        cls.exports = [
            "test_apply_stage_cannot_reverse_acknowledged_switch",
            "test_verify_stage_cannot_reverse_acknowledged_verification",
            "test_rollback_stage_cannot_reverse_acknowledged_rollback",
            "test_one_modules_stage_cannot_authorize_another_modules_change",
            "test_mixed_authorized_and_foreign_changes_are_rejected_atomically",
            "test_rollback_cannot_change_inactive_verification_flag",
            "test_forward_phases_preserve_unaffected_records_and_fields",
            "test_interrupted_phases_reconcile_both_observed_sides_without_slot_rewrite",
            "test_each_of_two_changed_modules_requires_its_own_stage",
            "test_verify_stage_cannot_authorize_rollback",
        ]
        cls.wasm = build_fixture("slot_transition_fixture.c", cls.directory, cls.exports,
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


for name in (
    "test_apply_stage_cannot_reverse_acknowledged_switch",
    "test_verify_stage_cannot_reverse_acknowledged_verification",
    "test_rollback_stage_cannot_reverse_acknowledged_rollback",
    "test_one_modules_stage_cannot_authorize_another_modules_change",
    "test_mixed_authorized_and_foreign_changes_are_rejected_atomically",
    "test_rollback_cannot_change_inactive_verification_flag",
    "test_forward_phases_preserve_unaffected_records_and_fields",
    "test_interrupted_phases_reconcile_both_observed_sides_without_slot_rewrite",
    "test_each_of_two_changed_modules_requires_its_own_stage",
    "test_verify_stage_cannot_authorize_rollback",
):
    setattr(T5SlotTransitionsC, name, case(name))
