"""Persistence-failure boundaries in the actual package ownership C code."""
import os
from pathlib import Path
import subprocess
import unittest

from test_health_policy import ROOT, build_fixture


class N4OwnershipCommit(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = Path(os.environ["KMZ_TEST_RUN_DIR"]) / "ownership-commit"
        cls.exports = ["test_owner_commit_retires_only_same_module",
                       "test_failed_commit_and_reconciliation_preserve_staged_intent"]
        cls.wasm = build_fixture("package_ownership_fixture.c", cls.directory, cls.exports,
                                  ["kernel/src/string.c"])

    def check(self, name):
        out = subprocess.run(["node", str(ROOT / "tests/kernel_fixture.js"),
                              str(self.wasm), name], capture_output=True, text=True)
        (self.directory / (name + ".txt")).write_text(out.stdout + out.stderr)
        self.assertEqual(out.returncode, 0, out.stdout + out.stderr)

    def test_owner_commit_retires_only_same_module(self):
        self.check(self.exports[0])

    def test_failed_commit_and_reconciliation_preserve_staged_intent(self):
        self.check(self.exports[1])
