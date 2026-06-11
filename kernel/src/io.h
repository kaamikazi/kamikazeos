/*
 * io.h — talking to hardware through x86 I/O ports.
 *
 * Besides normal memory, x86 has a second, separate address space called
 * "I/O ports" reached only with the special `in` and `out` instructions.
 * Old-school devices (PS/2 keyboard controller, serial port, VGA registers…)
 * live there. C has no syntax for these instructions, so we wrap them in
 * tiny inline-assembly helpers. These functions are the kernel's hands.
 */
#ifndef KMZ_IO_H
#define KMZ_IO_H

#include <stdint.h>

/* Read one byte from an I/O port. */
static inline uint8_t inb(uint16_t port)
{
    uint8_t value;
    /* "inb %dx, %al" — port number in DX, result arrives in AL. */
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

/* Write one byte to an I/O port. */
static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

/* 16-bit variants — the ATA data port moves data one WORD at a time. */
static inline uint16_t inw(uint16_t port)
{
    uint16_t value;
    __asm__ volatile ("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outw(uint16_t port, uint16_t value)
{
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

/*
 * Tiny delay (~1µs) used when slow hardware needs a moment between port
 * writes. Writing to port 0x80 is the traditional harmless way to do it.
 */
static inline void io_wait(void)
{
    outb(0x80, 0);
}

#endif /* KMZ_IO_H */
