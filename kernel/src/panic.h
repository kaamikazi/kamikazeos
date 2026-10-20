/*
 * panic.h — the kernel's "controlled crash".
 *
 * When the kernel detects a state it cannot safely continue from, hiding it
 * would violate KamikazeOS rule #5 ("unknown errors are never ignored").
 * kpanic() logs a structured PANIC event (which also streams to the serial
 * port, so the host has the evidence even though the machine is about to
 * halt), paints a diagnostic screen with the error code and recovery hint,
 * and stops the CPU safely in an infinite halt loop.
 */
#ifndef KMZ_PANIC_H
#define KMZ_PANIC_H

#include <stdint.h>

struct interrupt_frame;

/* Never returns. `code` is a stable ID like "TEST-PANIC-001". */
void kpanic(const char *code, const char *msg, const char *hint) __attribute__((noreturn));
void kpanic_exception(const char *name, const struct interrupt_frame *frame) __attribute__((noreturn));

/* v2.6.1: a test command calls this right before deliberately panicking
   or faulting, so the evidence is recorded as an intentional test (still
   logged, persisted, and reported - just not a production health fault). */
void kpanic_expect_intentional(void);
void kpanic_cancel_intentional(void);

/* Fixed built-in fault triggers only. If one returns, clear classification
   before reporting the unexpected return as a production failure. */
void kpanic_run_intentional_test(void (*trigger)(void));

#endif /* KMZ_PANIC_H */
