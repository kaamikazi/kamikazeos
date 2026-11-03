import os
from pathlib import Path
import subprocess
import unittest
from test_health_policy import ROOT, build_fixture

class T3ChronologyC(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory=Path(os.environ["KMZ_TEST_RUN_DIR"])/"journal-chronology"
        cls.names=["test_chronology_crosses_physical_sector_boundaries", "test_chronology_is_rechecked_not_sorted",
                   "test_failed_clear_never_unlocks_persistence"]
        cls.wasm=build_fixture("journal_chronology_fixture.c",cls.directory,cls.names,["kernel/src/string.c"])
    def check(self,name):
        out=subprocess.run(["node",str(ROOT/"tests/kernel_fixture.js"),str(self.wasm),name],capture_output=True,text=True)
        (self.directory/(name+".txt")).write_text(out.stdout+out.stderr)
        self.assertEqual(out.returncode,0,out.stdout+out.stderr)
    def test_chronology_crosses_physical_sector_boundaries(self): self.check(self.names[0])
    def test_chronology_is_rechecked_not_sorted(self): self.check(self.names[1])
    def test_failed_clear_never_unlocks_persistence(self): self.check(self.names[2])
