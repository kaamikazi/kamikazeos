#include <stdint.h>
#include "gdt.h"
#include "idt.h"

#define IDT_ENTRIES 256
#define INTERRUPT_GATE 0x8E

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t zero;
    uint8_t flags;
    uint16_t offset_high;
} __attribute__((packed));

struct idt_pointer {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

extern void (*interrupt_stub_table[])(void);
extern void isr_unknown(void);
extern void syscall_isr(void);

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_pointer idt_ptr;
static int loaded;
static int irq_gates_installed;
static int syscall_gate_installed;
static uint8_t syscall_gate_dpl;

static void idt_set_gate_dpl(uint8_t vector, void (*handler)(void), uint8_t dpl)
{
    uint32_t address = (uint32_t)handler;
    idt[vector].offset_low = (uint16_t)(address & 0xFFFFu);
    idt[vector].selector = GDT_KERNEL_CODE_SELECTOR;
    idt[vector].zero = 0;
    idt[vector].flags = (uint8_t)(INTERRUPT_GATE | ((dpl & 3u) << 5));
    idt[vector].offset_high = (uint16_t)(address >> 16);
}

static void idt_set_gate(uint8_t vector, void (*handler)(void))
{
    idt_set_gate_dpl(vector, handler, 0);
}

void idt_init_exceptions(void)
{
    loaded = 0;
    irq_gates_installed = 0;
    syscall_gate_installed = 0;
    syscall_gate_dpl = 0xFFu;
    for (uint32_t i = 0; i < IDT_ENTRIES; i++)
        idt_set_gate((uint8_t)i, isr_unknown);
    for (uint32_t i = 0; i < 32; i++)
        idt_set_gate((uint8_t)i, interrupt_stub_table[i]);

    idt_ptr.limit = (uint16_t)(sizeof(idt) - 1);
    idt_ptr.base = (uint32_t)&idt[0];
    __asm__ volatile ("lidt %0" : : "m"(idt_ptr));
    loaded = 1;
}

int idt_install_syscall_gate(uint8_t dpl)
{
    if (!loaded || (dpl != 0u && dpl != 3u))
        return 0;
    idt_set_gate_dpl(0x80u, syscall_isr, dpl);
    syscall_gate_installed = 1;
    syscall_gate_dpl = dpl;
    return 1;
}

void idt_install_irq_gates(void)
{
    if (!loaded)
        return;
    for (uint32_t i = 32; i < 48; i++)
        idt_set_gate((uint8_t)i, interrupt_stub_table[i]);
    irq_gates_installed = 1;
}

int idt_is_loaded(void)
{
    return loaded;
}

int idt_irq_gates_installed(void)
{
    return irq_gates_installed;
}

int idt_syscall_gate_installed(void)
{
    return syscall_gate_installed;
}

uint8_t idt_syscall_gate_dpl(void)
{
    return syscall_gate_installed ? syscall_gate_dpl : 0xFFu;
}
