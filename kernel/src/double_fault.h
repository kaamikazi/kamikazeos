#ifndef KMZ_DOUBLE_FAULT_H
#define KMZ_DOUBLE_FAULT_H
#include <stdint.h>
#include "x86_task.h"
struct double_fault_status {
    uint32_t prepared,descriptor_valid,gate_valid,state_valid,normal_selector;
    uint32_t selector,tss_address,cr3,entry,stack_start,stack_end;
    const char *reason;
};
int double_fault_init(void);
void double_fault_status(struct double_fault_status *);
const volatile struct x86_tss32 *double_fault_previous_state(void);
#endif
