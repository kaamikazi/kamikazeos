"""Compile actual kernel policy C for defensive host execution without I/O."""
import os
from pathlib import Path
import shutil
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]


def build_fixture(source, directory, exports, extra_sources=()):
    directory.mkdir(parents=True, exist_ok=True)
    clang = shutil.which("clang") or r"C:\Program Files\LLVM\bin\clang.exe"
    wasm = directory / (Path(source).stem + ".wasm")
    cmd = [clang, "--target=wasm32", "-ffreestanding", "-fno-builtin", "-nostdlib",
           "-O2", "-Wall", "-Wextra", "-Wl,--no-entry", str(ROOT / "tests" / source)]
    cmd += [str(ROOT / item) for item in extra_sources]
    cmd += [f"-Wl,--export={name}" for name in exports]
    cmd += ["-o", str(wasm)]
    out = subprocess.run(cmd, capture_output=True, text=True)
    (directory / "compile.txt").write_text(out.stdout + out.stderr)
    if out.returncode:
        raise RuntimeError(out.stdout + out.stderr)
    return wasm


class L2HealthPolicy(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = Path(os.environ["KMZ_TEST_RUN_DIR"]) / "health-policy"
        cls.exports = ["test_blocked_exact_verified_fallback",
                       "test_blocked_driver_without_fallback_scored_once",
                       "test_verified_exact_has_no_security_penalty"]
        cls.wasm = build_fixture("health_policy_fixture.c", cls.directory, cls.exports,
                                 ["kernel/src/errors.c", "kernel/src/string.c"])

    def check(self, name):
        out = subprocess.run(["node", str(ROOT / "tests" / "kernel_fixture.js"),
                              str(self.wasm), name], capture_output=True, text=True)
        (self.directory / (name + ".txt")).write_text(out.stdout + out.stderr)
        self.assertEqual(out.returncode, 0, out.stdout + out.stderr)

    def test_blocked_exact_verified_fallback(self):
        self.check(self.exports[0])

    def test_blocked_driver_without_fallback_scored_once(self):
        self.check(self.exports[1])

    def test_verified_exact_has_no_security_penalty(self):
        self.check(self.exports[2])
