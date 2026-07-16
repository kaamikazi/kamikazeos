#ifndef KMZ_RING3_H
#define KMZ_RING3_H

#include <stdint.h>

struct interrupt_frame;

struct ring3_status {
    int initialized;
    int execution_enabled;
    int test_running;
    int test_completed;
    int kernel_return_supported;
    int returned_to_kernel;
    int esp0_switch_observed;
    uint32_t user_code_start;
    uint32_t user_code_end;
    uint32_t user_stack_start;
    uint32_t user_stack_end;
    uint8_t last_cpl;
    uint32_t last_syscall;
    int32_t last_result;
    uint32_t user_faults_observed;
    uint32_t last_fault_vector;
    uint32_t last_fault_address;
    uint32_t last_fault_eip;
    uint32_t last_fault_user_esp;
    int last_fault_intentional;
    const char *test_status;
};

void ring3_init(void);
int ring3_run_test(void);
void ring3_run_fault_test(void);
void ring3_status(struct ring3_status *out);
int32_t ring3_last_result(void);
void ring3_note_syscall(uint8_t caller_cpl, uint32_t id, uint32_t kernel_esp);
void ring3_complete_user_exit(uint32_t result, uint32_t kernel_esp)
    __attribute__((noreturn));
void ring3_record_user_fault(uint32_t vector, uint32_t address, uint32_t eip,
                             uint32_t user_esp);

#endif /* KMZ_RING3_H */
