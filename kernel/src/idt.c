#include <stdint.h>
#include "idt.h"

#define IDT_ENTRIES 256
#define KERNEL_CODE_SELECTOR 0x08
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

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_pointer idt_ptr;
static int loaded;

static void idt_set_gate(uint8_t vector, void (*handler)(void))
{
    uint32_t address = (uint32_t)handler;
    idt[vector].offset_low = (uint16_t)(address & 0xFFFFu);
    idt[vector].selector = KERNEL_CODE_SELECTOR;
    idt[vector].zero = 0;
    idt[vector].flags = INTERRUPT_GATE;
    idt[vector].offset_high = (uint16_t)(address >> 16);
}

void idt_init(void)
{
    for (uint32_t i = 0; i < IDT_ENTRIES; i++)
        idt_set_gate((uint8_t)i, isr_unknown);
    for (uint32_t i = 0; i < 34; i++)
        idt_set_gate((uint8_t)i, interrupt_stub_table[i]);

    idt_ptr.limit = (uint16_t)(sizeof(idt) - 1);
    idt_ptr.base = (uint32_t)&idt[0];
    __asm__ volatile ("lidt %0" : : "m"(idt_ptr));
    loaded = 1;
}

int idt_is_loaded(void)
{
    return loaded;
}
