/* serial.c — polled COM1 (16550 UART) output driver. See serial.h. */
#include "serial.h"
#include "io.h"

/* COM1's I/O ports. The UART exposes registers at base+0 .. base+7. */
#define COM1            0x3F8
#define REG_DATA        (COM1 + 0)   /* read/write data byte               */
#define REG_INT_ENABLE  (COM1 + 1)   /* interrupt enable (we keep it off)  */
#define REG_BAUD_LO     (COM1 + 0)   /* baud divisor low  (when DLAB=1)    */
#define REG_BAUD_HI     (COM1 + 1)   /* baud divisor high (when DLAB=1)    */
#define REG_FIFO        (COM1 + 2)   /* FIFO control                       */
#define REG_LINE_CTRL   (COM1 + 3)   /* data bits/parity/stop + DLAB bit   */
#define REG_MODEM_CTRL  (COM1 + 4)   /* modem control (loopback lives here)*/
#define REG_LINE_STATUS (COM1 + 5)   /* bit 5 = transmitter ready          */

static int present;

void serial_init(void)
{
    outb(REG_INT_ENABLE, 0x00);   /* no interrupts — we poll              */
    outb(REG_LINE_CTRL,  0x80);   /* DLAB=1: next two writes set the baud */
    outb(REG_BAUD_LO,    0x03);   /* divisor 3 = 115200/3 = 38400 baud    */
    outb(REG_BAUD_HI,    0x00);
    outb(REG_LINE_CTRL,  0x03);   /* DLAB=0, 8 data bits, no parity, 1 stop */
    outb(REG_FIFO,       0xC7);   /* enable + clear FIFOs                 */

    /*
     * Self-test: put the UART in loopback mode (output wired to input),
     * send a byte, and check it comes back. GhostAttach thinking again:
     * detect the hardware, don't assume it.
     */
    outb(REG_MODEM_CTRL, 0x10);   /* loopback on */
    outb(REG_DATA, 0xAE);
    present = (inb(REG_DATA) == 0xAE);
    outb(REG_MODEM_CTRL, 0x03);   /* loopback off, normal operation */
}

int serial_present(void)
{
    return present;
}

void serial_putc(char c)
{
    if (!present)
        return;                              /* no UART: drop silently */
    while ((inb(REG_LINE_STATUS) & 0x20) == 0)
        ;                                    /* wait: transmitter busy */
    outb(REG_DATA, (uint8_t)c);
}

void serial_print(const char *s)
{
    while (*s) {
        if (*s == '\n')
            serial_putc('\r');               /* terminals want \r\n */
        serial_putc(*s++);
    }
}
