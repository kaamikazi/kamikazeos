#ifndef KMZ_INTERRUPTS_H
#define KMZ_INTERRUPTS_H

#include <stdint.h>

struct interrupt_frame {
    uint32_t edi, esi, ebp, esp;
    uint32_t ebx, edx, ecx, eax;
    uint32_t vector, error_code;
    uint32_t eip, cs, eflags;
    /* Present only when saved CS indicates a transition from CPL3. */
    uint32_t user_esp, user_ss;
};

void interrupts_init(void);
void interrupts_enable(void);
void interrupts_disable(void);
void interrupt_dispatch(struct interrupt_frame *frame);
uint32_t interrupts_handled(void);
uint32_t interrupts_last_vector(void);
uint32_t interrupts_last_exception(void);
uint32_t interrupts_last_error_code(void);
const char *interrupt_exception_name(uint32_t vector);
int interrupts_are_enabled(void);
void interrupts_trigger_divide_test(void);
void interrupts_trigger_invalid_opcode_test(void);

#endif
