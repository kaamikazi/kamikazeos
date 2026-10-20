"""Production descriptor/frame/ATA tests; no hardware task switch or CPU fault."""
import os
from pathlib import Path
import subprocess
import unittest
from test_health_policy import ROOT, build_fixture

ARCH_CASES=(
    "test_tss_descriptors_encode_fixed_supervisor_state",
    "test_descriptor_mutations_and_busy_policy_are_checked",
    "test_df_task_gate_is_exact_dpl0_selector_only",
    "test_df_task_state_has_valid_cr3_stack_and_integer_entry",
    "test_df_task_state_every_byte_is_structurally_checked",
    "test_invalid_task_parameters_are_refused",
    "test_kernel_frame_without_error_preserves_accepted_abi",
    "test_kernel_error_frame_captures_early_cr2_and_exact_code",
    "test_user_tail_requires_privilege_stack_and_valid_selectors",
    "test_short_bad_or_unclassified_frames_are_not_followed",
    "test_same_cpl_never_uses_poisoned_user_tail",
    "test_error_shape_and_conditional_tail_bounds_are_checked",
)
ATA_CASES=(
    "test_ata_guard_covers_initialization_reads_and_writes",
    "test_ata_guard_is_released_on_retry_failure_and_invalid_requests",
    "test_absent_disk_initialization_does_not_leave_io_guard_active",
)
class ArchFixture(unittest.TestCase):
    def check(self,name):
        p=subprocess.run(["node",str(ROOT/"tests/kernel_fixture.js"),str(self.wasm),name],capture_output=True,text=True)
        (self.directory/(name+".txt")).write_text(p.stdout+p.stderr)
        self.assertEqual(p.returncode,0,p.stdout+p.stderr)
class B3ArchitectureC(ArchFixture):
    @classmethod
    def setUpClass(cls):
        cls.directory=Path(os.environ["KMZ_TEST_RUN_DIR"])/"emergency-architecture-actual"
        cls.wasm=build_fixture("emergency_arch_fixture.c",cls.directory,ARCH_CASES,
            ["kernel/src/x86_task.c","kernel/src/emergency_frame.c","kernel/src/memory_policy.c","kernel/src/string.c"])
class B4AtaGuardC(ArchFixture):
    @classmethod
    def setUpClass(cls):
        cls.directory=Path(os.environ["KMZ_TEST_RUN_DIR"])/"emergency-ata-actual"
        cls.wasm=build_fixture("emergency_ata_fixture.c",cls.directory,ATA_CASES)
def case(name):
    def run(self):self.check(name)
    return run
for name in ARCH_CASES:setattr(B3ArchitectureC,name,case(name))
for name in ATA_CASES:setattr(B4AtaGuardC,name,case(name))
