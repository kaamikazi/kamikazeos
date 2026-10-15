import os
from pathlib import Path
import subprocess
import unittest
from test_health_policy import ROOT, build_fixture

class T1SnapshotPreflight(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = Path(os.environ["KMZ_TEST_RUN_DIR"]) / "snapshot-preflight"
        cls.wasm = build_fixture("snapshot_preflight_fixture.c", cls.directory,
            ["test_swapped_and_duplicate_history_is_held", "test_ring_age_and_boot_boundaries"],
            ["kernel/src/string.c"])
    def check(self, name):
        out = subprocess.run(["node", str(ROOT / "tests/kernel_fixture.js"), str(self.wasm), name],
                             capture_output=True, text=True)
        (self.directory / (name + ".txt")).write_text(out.stdout + out.stderr)
        self.assertEqual(out.returncode, 0, out.stdout + out.stderr)
    def test_swapped_and_duplicate_history_is_held(self): self.check("test_swapped_and_duplicate_history_is_held")
    def test_ring_age_and_boot_boundaries(self): self.check("test_ring_age_and_boot_boundaries")
