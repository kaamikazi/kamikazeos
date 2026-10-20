"""B1 production-C record/policy/UART tests; no guest exception execution."""
import os
from pathlib import Path
import subprocess
import unittest
from test_health_policy import ROOT, build_fixture

RECORD_CASES = (
    "test_verified_ordinary_faults_and_panics_are_eligible",
    "test_emergency_reentry_and_df_never_persist",
    "test_io_unready_or_untrusted_storage_refuses_persistence",
    "test_bad_or_exhausted_stack_bounds_are_unsafe",
    "test_unvalidated_origin_and_cpu_state_are_unsafe",
    "test_guard_and_paging_faults_never_persist",
    "test_fixed_record_captures_once_and_preserves_first_evidence",
    "test_formatter_has_fixed_output_and_stops_after_sink_failure",
)
UART_CASES = (
    "test_uart_stuck_polling_and_oversized_budgets_are_bounded",
    "test_uart_ready_absent_and_zero_budget_paths",
    "test_uart_strings_and_fields_have_hard_caps",
    "test_uart_crlf_and_timeout_string_termination",
)

class B1FixtureTests(unittest.TestCase):
    def check(self,name):
        p=subprocess.run(["node",str(ROOT/"tests/kernel_fixture.js"),str(self.wasm),name],capture_output=True,text=True)
        (self.directory/(name+".txt")).write_text(p.stdout+p.stderr)
        self.assertEqual(p.returncode,0,p.stdout+p.stderr)

class B1RecordC(B1FixtureTests):
    @classmethod
    def setUpClass(cls):
        cls.directory=Path(os.environ["KMZ_TEST_RUN_DIR"])/"emergency-record-actual"
        cls.wasm=build_fixture("emergency_record_fixture.c",cls.directory,RECORD_CASES,
                               ["kernel/src/emergency_record.c","kernel/src/string.c"])
class B1UartC(B1FixtureTests):
    @classmethod
    def setUpClass(cls):
        cls.directory=Path(os.environ["KMZ_TEST_RUN_DIR"])/"emergency-uart-actual"
        cls.wasm=build_fixture("emergency_uart_fixture.c",cls.directory,UART_CASES)

def case(name):
    def run(self):self.check(name)
    return run
for name in RECORD_CASES:setattr(B1RecordC,name,case(name))
for name in UART_CASES:setattr(B1UartC,name,case(name))
