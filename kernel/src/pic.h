#ifndef KMZ_PIC_H
#define KMZ_PIC_H

#include <stdint.h>

void pic_init(void);
void pic_send_eoi(uint8_t irq);
void pic_enable_irq(uint8_t irq);
void pic_disable_irq(uint8_t irq);
int pic_is_remapped(void);
int pic_irq_enabled(uint8_t irq);

#endif
