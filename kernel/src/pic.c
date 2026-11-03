#include "pic.h"
#include "io.h"

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1
#define PIC_EOI      0x20
#define ICW1_INIT    0x10
#define ICW1_ICW4    0x01
#define ICW4_8086    0x01

static uint16_t irq_mask = 0xFFFFu;
static int remapped;

static void write_masks(void)
{
    outb(PIC1_DATA, (uint8_t)(irq_mask & 0xFFu));
    outb(PIC2_DATA, (uint8_t)(irq_mask >> 8));
}

void pic_init(void)
{
    __asm__ volatile ("cli");
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4); io_wait();
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4); io_wait();
    outb(PIC1_DATA, 0x20); io_wait();
    outb(PIC2_DATA, 0x28); io_wait();
    outb(PIC1_DATA, 0x04); io_wait();
    outb(PIC2_DATA, 0x02); io_wait();
    outb(PIC1_DATA, ICW4_8086); io_wait();
    outb(PIC2_DATA, ICW4_8086); io_wait();

    irq_mask = 0xFFFFu;
    write_masks();
    remapped = 1;
}

void pic_send_eoi(uint8_t irq)
{
    if (irq >= 8)
        outb(PIC2_COMMAND, PIC_EOI);
    outb(PIC1_COMMAND, PIC_EOI);
}

void pic_enable_irq(uint8_t irq)
{
    if (irq >= 16)
        return;
    irq_mask &= (uint16_t)~(1u << irq);
    if (irq >= 8)
        irq_mask &= (uint16_t)~(1u << 2);
    write_masks();
}

void pic_disable_irq(uint8_t irq)
{
    if (irq >= 16)
        return;
    irq_mask |= (uint16_t)(1u << irq);
    write_masks();
}

int pic_is_remapped(void)
{
    return remapped;
}

int pic_irq_enabled(uint8_t irq)
{
    return irq < 16 && (irq_mask & (1u << irq)) == 0;
}
