#ifndef KMZ_PRIVILEGE_H
#define KMZ_PRIVILEGE_H

#include <stdint.h>

struct privilege_status {
    int gdt_loaded;
    int user_segments_valid;
    int tss_loaded;
    int kernel_stack_valid;
    int paging_enabled;
    int syscall_gate_installed;
    uint8_t syscall_gate_dpl;
    int foundation_ready;
    int execution_enabled;
    uint8_t current_cpl;
    uint32_t bootstrap_stack_bottom;
    uint32_t bootstrap_stack_top;
    uint32_t transition_stack_bottom;
    uint32_t transition_stack_top;
    uint32_t esp0;
    const char *guard_policy;
};

void privilege_init(void);
int privilege_ready_for_ring3(void);
void privilege_status(struct privilege_status *out);

#endif /* KMZ_PRIVILEGE_H */
