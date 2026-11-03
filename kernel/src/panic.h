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

/* Never returns. `code` is a stable ID like "TEST-PANIC-001". */
void kpanic(const char *code, const char *msg, const char *hint);
void kpanic_exception(uint32_t vector, uint32_t error_code,
                      const char *name, uint32_t eip);

#endif /* KMZ_PANIC_H */
