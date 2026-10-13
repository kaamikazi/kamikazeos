#include "ring3.h"
#include "errors.h"
#include "gdt.h"
#include "paging.h"
#include "panic.h"
#include "privilege.h"
#include "string.h"
#include "syscall.h"
#include "tss.h"

/* Default release posture; a disposable test build may temporarily set this. */
#define RING3_ROUNDTRIP_TEST_APPROVED 0

extern char _user_code_start;
extern char _user_code_end;
extern char _user_stack_start;
extern char _user_stack_end;
extern void ring3_user_test_start(void);
extern void ring3_user_fault_start(void);
extern void ring3_enter_user(uint32_t eip, uint32_t esp);
extern void ring3_return_to_kernel(void) __attribute__((noreturn));

static struct ring3_status state;

static int kernel_esp_on_transition_stack(uint32_t esp)
{
    return esp >= tss_privilege_stack_bottom() &&
           esp < tss_privilege_stack_top();
}

void ring3_init(void)
{
    struct user_memory_status memory;
    kmemset(&state, 0, sizeof(state));
    paging_user_memory_status(&memory);
    state.initialized = 1;
    state.kernel_return_supported = 1;
    state.user_code_start = memory.code_start;
    state.user_code_end = memory.code_end;
    state.user_stack_start = memory.stack_start;
    state.user_stack_end = memory.stack_end;
    state.last_cpl = 0;
    state.last_syscall = 0xFFFFFFFFu;
    state.test_status = "never_run";
    state.prerequisites_valid = privilege_ready_for_ring3() && memory.validated &&
        (uint32_t)&ring3_user_test_start >= memory.code_start &&
        (uint32_t)&ring3_user_test_start < memory.code_end &&
        (uint32_t)&ring3_user_fault_start >= memory.code_start &&
        (uint32_t)&ring3_user_fault_start < memory.code_end &&
        memory.stack_end - memory.stack_start == 16384u;
    state.execution_approved =
        state.prerequisites_valid && RING3_ROUNDTRIP_TEST_APPROVED;
    state.execution_enabled = state.execution_approved;
    state.fault_test_approved = 0;
    error_log(state.execution_enabled ? SEV_INFO :
              (state.prerequisites_valid ? SEV_INFO : SEV_WARNING),
              state.execution_enabled ? "RING3-READY" :
              (state.prerequisites_valid ? "RING3-HOLD" : "RING3-101"),
              "Ring3",
              state.execution_enabled
                ? "One fixed CPL3 round trip approved for this test build"
                : (state.prerequisites_valid
                    ? "CPL3 prerequisites valid; execution held for re-audit"
                    : "CPL3 execution disabled by readiness validation"),
              state.execution_enabled
                ? "approval is consumed before entry; CPL3 fault test remains disabled"
                : (state.prerequisites_valid
                    ? "explicit approval is required before enabling any transition"
                    : "inspect user-memory and ring3-readiness"));
}

int ring3_run_test(void)
{
    if (!state.execution_enabled || state.test_running) {
        error_log_class(SEV_WARNING, HEALTH_OPERATOR_INPUT, "RING3-102", "Ring3",
                        "Ring 3 test refused", "check readiness and active test state");
        return 0;
    }
    state.test_running = 1;
    state.test_completed = 0;
    state.returned_to_kernel = 0;
    state.esp0_switch_observed = 0;
    state.context_preservation_verified = 0;
    state.test_status = "running";
    /* Consume approval before IRET so no second transition can be requested. */
    state.approval_consumed = 1;
    state.execution_approved = 0;
    state.execution_enabled = 0;
    ring3_enter_user((uint32_t)&ring3_user_test_start,
                     (uint32_t)&_user_stack_end);
    if (!state.test_completed || !state.returned_to_kernel) {
        state.test_running = 0;
        state.test_status = "invalid_return";
        error_log(SEV_ERROR, "RING3-103", "Ring3",
                  "Ring 3 return context was incomplete",
                  "disable further tests and inspect syscall evidence");
        state.execution_enabled = 0;
        return 0;
    }
    error_log(SEV_INFO, "RING3-001", "Ring3",
              "CPL3 test returned through SYS_USER_EXIT",
              "kernel shell resumed on its original Ring 0 stack");
    return state.last_result == 0;
}

void ring3_run_fault_test(void)
{
    if (!state.fault_test_approved || state.test_running) {
        kpanic_cancel_intentional();
        error_log_class(SEV_WARNING, HEALTH_OPERATOR_INPUT, "RING3-102", "Ring3",
                  "Ring 3 fault test refused",
                  "fault-test approval is independent and remains disabled");
        return;
    }
    state.test_running = 1;
    state.test_completed = 0;
    state.returned_to_kernel = 0;
    state.test_status = "intentional_fault_running";
    ring3_enter_user((uint32_t)&ring3_user_fault_start,
                     (uint32_t)&_user_stack_end);
    state.test_running = 0;
    state.test_status = "fault_test_unexpected_return";
}

void ring3_note_syscall(uint8_t caller_cpl, uint32_t id, uint32_t kernel_esp)
{
    state.last_cpl = caller_cpl;
    state.last_syscall = id;
    if (caller_cpl == 3u && kernel_esp_on_transition_stack(kernel_esp))
        state.esp0_switch_observed = 1;
}

void ring3_complete_user_exit(uint32_t result, uint32_t kernel_esp)
{
    state.last_cpl = 3;
    state.last_syscall = SYS_USER_EXIT;
    state.last_result = (int32_t)result;
    state.esp0_switch_observed = kernel_esp_on_transition_stack(kernel_esp);
    state.test_running = 0;
    state.test_completed = 1;
    state.returned_to_kernel = 1;
    state.context_preservation_verified = result == 0;
    state.test_status = result == 0 ? "passed" : "failed";
    ring3_return_to_kernel();
}

void ring3_record_user_fault(uint32_t vector, uint32_t address, uint32_t eip,
                             uint32_t user_esp)
{
    state.last_fault_intentional =
        kstrcmp(state.test_status, "intentional_fault_running") == 0;
    state.last_cpl = 3;
    state.user_faults_observed++;
    state.last_fault_vector = vector;
    state.last_fault_address = address;
    state.last_fault_eip = eip;
    state.last_fault_user_esp = user_esp;
    state.test_running = 0;
    state.test_status = "cpl3_fault_halted";
}

void ring3_status(struct ring3_status *out)
{
    if (out)
        *out = state;
}

int32_t ring3_last_result(void)
{
    return state.last_result;
}
