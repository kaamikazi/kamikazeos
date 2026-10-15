"""Persistence-failure boundaries in the actual package ownership C code."""
import os
from pathlib import Path
import subprocess
import unittest

from test_health_policy import ROOT, build_fixture


class JointFixtureTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = Path(os.environ["KMZ_TEST_RUN_DIR"]) / "ownership-joint-actual"
        cls.exports = ["test_owner_commit_retires_only_same_module",
                       "test_failed_commit_and_reconciliation_preserve_staged_intent",
                       "test_real_slot_failure_retains_stage_until_cold_read",
                       "test_slot_read_failure_cannot_rewrite_staged_intent",
                       "test_reload_cannot_clear_latched_uncertainty_with_stale_copies",
                       "test_candidate_approval_cannot_destroy_current_owner",
                       "test_scoped_reset_and_partial_reset_hold",
                       "test_coherent_owner_transfer_still_requires_durable_intent",
                       "test_slot_flags_are_not_normalized_into_authority",
                       "test_case_aliases_cannot_split_package_authority"]
        cls.wasm = build_fixture("joint_authority_fixture.c", cls.directory, cls.exports,
                                  ["kernel/src/string.c", "kernel/src/package_state.c",
                                   "kernel/src/module_slots.c", "kernel/src/module_versions.c"])

    def check(self, name):
        out = subprocess.run(["node", str(ROOT / "tests/kernel_fixture.js"),
                              str(self.wasm), name], capture_output=True, text=True)
        (self.directory / (name + ".txt")).write_text(out.stdout + out.stderr)
        self.assertEqual(out.returncode, 0, out.stdout + out.stderr)

class N4OwnershipCommit(JointFixtureTests):
    def test_owner_commit_retires_only_same_module(self):
        self.check(self.exports[0])

    def test_failed_commit_and_reconciliation_preserve_staged_intent(self):
        self.check(self.exports[1])

class T4JointAuthorityC(JointFixtureTests):
    def test_real_slot_failure_retains_stage_until_cold_read(self): self.check(self.exports[2])
    def test_slot_read_failure_cannot_rewrite_staged_intent(self): self.check(self.exports[3])
    def test_reload_cannot_clear_latched_uncertainty_with_stale_copies(self): self.check(self.exports[4])
    def test_candidate_approval_cannot_destroy_current_owner(self): self.check(self.exports[5])
    def test_scoped_reset_and_partial_reset_hold(self): self.check(self.exports[6])
    def test_coherent_owner_transfer_still_requires_durable_intent(self): self.check(self.exports[7])
    def test_slot_flags_are_not_normalized_into_authority(self): self.check(self.exports[8])
    def test_case_aliases_cannot_split_package_authority(self): self.check(self.exports[9])
