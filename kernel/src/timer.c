#include "timer.h"
#include "io.h"
#include "pic.h"

#define PIT_COMMAND 0x43
#define PIT_CHANNEL0 0x40
#define PIT_BASE_HZ 1193182u

static volatile uint32_t ticks;
static int enabled;

void timer_init(uint32_t frequency_hz)
{
    if (frequency_hz < 19)
        frequency_hz = 19;
    if (frequency_hz > 1000)
        frequency_hz = 1000;

    uint32_t divisor = PIT_BASE_HZ / frequency_hz;
    outb(PIT_COMMAND, 0x36);
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFFu));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFFu));
    ticks = 0;
    pic_enable_irq(0);
    enabled = 1;
}

void timer_handle_irq(void)
{
    ticks++;
}

uint32_t timer_ticks(void)
{
    return ticks;
}

int timer_is_enabled(void)
{
    return enabled && pic_irq_enabled(0);
}
