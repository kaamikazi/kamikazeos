#ifndef KMZ_TSS_H
#define KMZ_TSS_H

#include <stdint.h>
struct x86_tss32;

#define TSS_PRIVILEGE_STACK_SIZE 16384u

struct tss_status {
    int initialized;
    int descriptor_valid;
    int loaded;
    int kernel_stack_valid;
    uint16_t selector;
    uint16_t ss0;
    uint32_t esp0;
    uint32_t stack_bottom;
    uint32_t stack_top;
    uint32_t stack_size;
    const char *guard_policy;
};

void tss_init(void);
int tss_set_esp0(uint32_t esp0);
int tss_is_loaded(void);
const volatile struct x86_tss32 *tss_normal_state(void); /* CPU-written previous-task evidence */
int tss_kernel_stack_valid(void);
uint32_t tss_privilege_stack_bottom(void);
uint32_t tss_privilege_stack_top(void);
void tss_status(struct tss_status *out);

#endif /* KMZ_TSS_H */
