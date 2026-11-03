"""Unexpected-return scope test plus real Ring 0 fault regressions."""
import os
from pathlib import Path
import subprocess
import time
import unittest

from test_health_policy import ROOT, build_fixture
from test_integrity_gate import KmzCase
from kmz_qemu import section_kv


class L3PanicScope(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = Path(os.environ["KMZ_TEST_RUN_DIR"]) / "panic-scope"
        cls.exports = ["test_returning_trigger_clears_classification",
                       "test_null_trigger_does_not_arm_classification"]
        cls.wasm = build_fixture("panic_scope_fixture.c", cls.directory, cls.exports)

    def check(self, name):
        out = subprocess.run(["node", str(ROOT / "tests" / "kernel_fixture.js"),
                              str(self.wasm), name], capture_output=True, text=True)
        (self.directory / (name + ".txt")).write_text(out.stdout + out.stderr)
        self.assertEqual(out.returncode, 0, out.stdout + out.stderr)

    def test_returning_trigger_clears_classification(self):
        self.check(self.exports[0])

    def test_null_trigger_does_not_arm_classification(self):
        self.check(self.exports[1])


class L3Ring0Faults(KmzCase):
    def check_fault(self, command, code, vector):
        with self.boot() as q:
            start = q.serial_size()
            q.run(command, rf"code={code}[;]")
            q.wait_for(r"caller_cpl=0;user_stack_present=no", since=start)
            time.sleep(0.3)  # allow the panic-state flush following the serial frame
            text = q.read_serial()[start:]
            self.assertIn("health=excluded_intentional_test", text)
            self.assertIn(f"vector=0x{vector:08X}", text)
            self.assertIn("caller_cpl=0", text)
            self.assertIn("user_stack_present=no", text)
        rep = self.boot_once()
        memory = section_kv(rep, "RECOVERY_MEMORY")
        self.assertEqual(memory["last_boot_panic"], "1")
        self.assertEqual(memory["last_boot_panic_intentional"], "1")
        self.assertEqual(section_kv(rep, "CURRENT_HEALTH")["panic_penalty"], "0")
        self.assertEqual(section_kv(rep, "RING3")["execution_approved"], "no")

    def test_divide_fault(self):
        self.check_fault("exceptions-test divide confirm", "CPU-EXCEPTION", 0)

    def test_invalid_opcode_fault(self):
        self.check_fault("exceptions-test invalid-opcode confirm", "CPU-EXCEPTION", 6)

    def test_page_fault(self):
        self.check_fault("pagefault-test confirm", "PAGE-FAULT", 14)
