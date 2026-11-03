#ifndef KMZ_TIMER_H
#define KMZ_TIMER_H

#include <stdint.h>

void timer_init(uint32_t frequency_hz);
void timer_handle_irq(void);
uint32_t timer_ticks(void);
int timer_is_enabled(void);

#endif
