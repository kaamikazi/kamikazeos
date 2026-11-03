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

int serial_putc_bounded(char c, uint32_t polls)
{
    if (!present) return 0;
    if (polls>SERIAL_MAX_POLLS) polls=SERIAL_MAX_POLLS;
    for (uint32_t i=0;i<polls;i++) if (inb(REG_LINE_STATUS)&0x20) {
        outb(REG_DATA,(uint8_t)c);return 1;
    }
    return 0;
}

void serial_putc(char c) { (void)serial_putc_bounded(c,SERIAL_MAX_POLLS); }

uint32_t serial_print_bounded(const char *s, uint32_t chars, uint32_t polls)
{
    if (!s) return 0;
    if (chars>SERIAL_MAX_STRING) chars=SERIAL_MAX_STRING;
    uint32_t i=0;
    for (;i<chars && s[i];i++) {
        if (s[i]=='\n' && !serial_putc_bounded('\r',polls)) break;
        if (!serial_putc_bounded(s[i],polls)) break;
    }
    return i;
}

void serial_print(const char *s) { (void)serial_print_bounded(s,SERIAL_MAX_STRING,SERIAL_MAX_POLLS); }

void serial_print_field(const char *s)
{
    if (!s) {
        serial_print("?");
        return;
    }
    for (uint32_t i=0;i<SERIAL_MAX_STRING && s[i];i++) {
        unsigned char c = (unsigned char)s[i];
        char value=(c < 0x20 || c == 0x7f || c == ';' || c == '=') ? '?' : (char)c;
        if (!serial_putc_bounded(value,SERIAL_MAX_POLLS)) break;
    }
}
