"""Host execution of actual shared journal field-domain validation."""
import os
from pathlib import Path
import subprocess
import unittest
from test_health_policy import ROOT, build_fixture


class R5JournalDomains(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = Path(os.environ["KMZ_TEST_RUN_DIR"]) / "journal-domains"
        cls.wasm = build_fixture("journal_domains_fixture.c", cls.directory,
                                 ["test_documented_journal_domains", "test_valid_journal_boundaries"],
                                 ["kernel/src/string.c"])

    def check(self, name):
        out = subprocess.run(["node", str(ROOT / "tests/kernel_fixture.js"), str(self.wasm), name],
                             capture_output=True, text=True)
        (self.directory / (name + ".txt")).write_text(out.stdout + out.stderr)
        self.assertEqual(out.returncode, 0, out.stdout + out.stderr)

    def test_documented_journal_domains(self): self.check("test_documented_journal_domains")
    def test_valid_journal_boundaries(self): self.check("test_valid_journal_boundaries")
