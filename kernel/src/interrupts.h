#ifndef KMZ_INTERRUPTS_H
#define KMZ_INTERRUPTS_H

#include <stdint.h>

struct interrupt_frame {
    /* Explicit 32-bit copies pushed by the common assembly entry. */
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp;
    uint32_t ebx, edx, ecx, eax;
    uint32_t vector, error_code;
    uint32_t eip, cs, eflags;
};

/*
 * The CPU appends this tail only when an interrupt changes privilege level.
 * KamikazeOS currently accepts CPL3-to-CPL0 as the only such transition.
 * Ring 0 faults end at interrupt_frame. Code must use the helpers below and
 * must never read this tail for a same-CPL frame.
 */
struct interrupt_user_stack {
    uint32_t esp;
    uint32_t ss;
};

void interrupts_init(void);
void interrupts_enable(void);
void interrupts_disable(void);
void interrupt_dispatch(struct interrupt_frame *frame);
uint8_t interrupt_frame_caller_cpl(const struct interrupt_frame *frame);
int interrupt_frame_user_stack(const struct interrupt_frame *frame,
                               uint32_t *user_esp, uint32_t *user_ss);
int interrupt_frame_layout_self_test(void);
uint32_t interrupts_handled(void);
uint32_t interrupts_last_vector(void);
uint32_t interrupts_last_exception(void);
uint32_t interrupts_last_error_code(void);
const char *interrupt_exception_name(uint32_t vector);
int interrupts_are_enabled(void);
void interrupts_trigger_divide_test(void);
void interrupts_trigger_invalid_opcode_test(void);

#endif
