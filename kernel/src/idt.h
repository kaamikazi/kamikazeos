#ifndef KMZ_IDT_H
#define KMZ_IDT_H

#include <stdint.h>

void idt_init_exceptions(void);
void idt_install_irq_gates(void);
int idt_install_syscall_gate(uint8_t dpl);
int idt_is_loaded(void);
int idt_irq_gates_installed(void);
int idt_syscall_gate_installed(void);
uint8_t idt_syscall_gate_dpl(void);
int idt_install_double_fault_gate(void);
int idt_double_fault_gate_valid(void);

#endif
