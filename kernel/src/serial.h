/*
 * serial.h — COM1 serial port output.
 *
 * WHY A SERIAL PORT IN 2026? Because it is the simplest possible pipe from
 * inside the VM to a file on the host. Run QEMU with
 *     -serial file:build/serial.log
 * and every byte the kernel writes to COM1 lands in that file — surviving
 * panics, reboots, and the screen scrolling away. This is the channel the
 * AI Recovery Core (Phase 4) reads. The screen is for humans; the serial
 * port is for machines.
 *
 * We use polling (check "transmitter empty" bit, then write a byte) — no
 * interrupts needed for output, which keeps v0.2 simple and safe.
 */
#ifndef KMZ_SERIAL_H
#define KMZ_SERIAL_H

#include <stdint.h>
#define SERIAL_MAX_POLLS 100000u
#define SERIAL_MAX_STRING 4096u

void serial_init(void);             /* configure COM1: 38400 baud, 8N1 */
int  serial_present(void);          /* 1 if loopback self-test passed  */
void serial_putc(char c);
/* Finite per-character budget; no timer, heap, error log or persistence calls. */
int serial_putc_bounded(char c, uint32_t polls);
uint32_t serial_print_bounded(const char *s, uint32_t chars, uint32_t polls);
void serial_print(const char *s);
void serial_print_field(const char *s);

#endif /* KMZ_SERIAL_H */
