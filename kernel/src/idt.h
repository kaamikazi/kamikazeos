#ifndef KMZ_IDT_H
#define KMZ_IDT_H

void idt_init_exceptions(void);
void idt_install_irq_gates(void);
int idt_is_loaded(void);
int idt_irq_gates_installed(void);

#endif
