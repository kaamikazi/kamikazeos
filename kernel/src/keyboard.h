/*
 * keyboard.h — PS/2 keyboard input.
 *
 * QEMU emulates the classic PS/2 keyboard controller (Intel 8042), reachable
 * at I/O ports 0x60 (data) and 0x64 (status/command). When you press or
 * release a key, the controller queues a "scancode" byte we can read.
 *
 * Phase 1 uses POLLING: we sit in a loop asking "got a byte yet?". Simple
 * and correct, but burns CPU. Phase 2 replaces this with interrupt-driven
 * input (IRQ1), which is how real systems work.
 */
#ifndef KMZ_KEYBOARD_H
#define KMZ_KEYBOARD_H

void keyboard_init(void);
void keyboard_enable_irq(void);
void keyboard_handle_irq(void);

/* Block until a printable char / Enter ('\n') / Backspace ('\b') arrives. */
char keyboard_getchar(void);

/* 1 if a PS/2 controller responded to our probe at boot (for `devices`). */
int keyboard_present(void);
int keyboard_irq_enabled(void);
const char *keyboard_mode_name(void);

#endif /* KMZ_KEYBOARD_H */
