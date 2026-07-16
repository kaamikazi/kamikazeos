#include "privilege.h"
#include "errors.h"
#include "gdt.h"
#include "idt.h"
#include "paging.h"
#include "tss.h"

extern char stack_bottom;
extern char stack_top;

static uint8_t current_cpl(void)
{
    uint16_t cs;
    __asm__ volatile ("mov %%cs, %0" : "=r"(cs));
    return (uint8_t)(cs & 3u);
}

int privilege_ready_for_ring3(void)
{
    return gdt_is_loaded() && gdt_user_segments_valid() &&
           tss_is_loaded() && tss_kernel_stack_valid() &&
           paging_is_enabled() && idt_syscall_gate_installed() &&
           idt_syscall_gate_dpl() == 3u;
}

void privilege_init(void)
{
    int ready = privilege_ready_for_ring3();
    error_log(ready ? SEV_INFO : SEV_WARNING,
              ready ? "PRIV-000" : "PRIV-101", "Privilege",
              ready ? "Ring 3 architectural foundation validated"
                    : "Ring 3 foundation incomplete; user gate disabled",
              ready ? "execution remains disabled until user mappings exist"
                    : "inspect GDT TSS paging and syscall gate status");
}

void privilege_status(struct privilege_status *out)
{
    if (!out)
        return;
    struct tss_status tss;
    tss_status(&tss);
    out->gdt_loaded = gdt_is_loaded();
    out->user_segments_valid = gdt_user_segments_valid();
    out->tss_loaded = tss.loaded;
    out->kernel_stack_valid = tss.kernel_stack_valid;
    out->paging_enabled = paging_is_enabled();
    out->syscall_gate_installed = idt_syscall_gate_installed();
    out->syscall_gate_dpl = idt_syscall_gate_dpl();
    out->foundation_ready = privilege_ready_for_ring3();
    out->execution_enabled = 0;
    out->current_cpl = current_cpl();
    out->bootstrap_stack_bottom = (uint32_t)&stack_bottom;
    out->bootstrap_stack_top = (uint32_t)&stack_top;
    out->transition_stack_bottom = tss.stack_bottom;
    out->transition_stack_top = tss.stack_top;
    out->esp0 = tss.esp0;
    out->guard_policy = tss.guard_policy;
}
