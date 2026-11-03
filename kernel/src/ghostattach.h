/*
 * ghostattach.h — GhostAttach, the driver recovery layer of KamikazeOS.
 *
 * GhostAttach answers one question for every device, at every boot:
 * "which driver may safely operate this hardware?" — and answers it with
 * a DETERMINISTIC POLICY, not guesses:
 *
 *   1. Prefer the exact verified driver for the device.
 *   2. If the exact driver is missing, use a VERIFIED fallback driver.
 *   3. Unverified drivers are BLOCKED. Always. No exceptions.
 *   4. No verified driver at all -> device marked missing_driver (off).
 *   5. A driver that fails to attach -> device ISOLATED (quarantined).
 *   6. Every decision is logged with a stable GA-* error code, so the
 *      AI Recovery Core can later reason over the evidence.
 *
 * There is deliberately NO AI inside this layer. The AI may eventually
 * *advise* ("a verified driver for this device exists upstream"), but it
 * goes through this policy like everyone else — it can never bypass it.
 *
 * v0.3 scope: static device/driver registries (no PCI scan yet), attach
 * decisions are real policy over simulated hardware — except PS/2 and
 * COM1, where the real probe results feed the scan.
 */
#ifndef KMZ_GHOSTATTACH_H
#define KMZ_GHOSTATTACH_H

#include <stdint.h>
#include "device.h"
#include "driver.h"

/* Run the boot-time scan: walk every device, apply the policy, log.
 * In safe mode (safe_mode != 0) only ESSENTIAL devices attach — display,
 * keyboard, serial. Everything else is isolated without a driver attempt,
 * so a misbehaving non-essential driver cannot take the boot down. */
void ghostattach_scan(int safe_mode);

/* Read-only access to the registries, for the shell. */
uint32_t             ghostattach_device_count(void);
const struct device *ghostattach_device(uint32_t index);   /* 0 if bad index */
uint32_t             ghostattach_driver_count(void);
const struct driver *ghostattach_driver(uint32_t index);   /* 0 if bad index */

/*
 * State mutators for the RecoveryEngine (v0.5). They act on the FIRST
 * device of the given type — fine while the registry has one device per
 * type; revisit when real PCI scanning brings duplicates.
 * Note: these are how APPROVED recovery actions change device state.
 * Nothing else in the kernel mutates the registry after the boot scan.
 */
enum device_status ghostattach_get_status_by_type(enum device_type type);
void               ghostattach_set_status_by_type(enum device_type type,
                                                  enum device_status status);

/* Summary counters filled in by the scan (for `devices` and boot output). */
struct ghostattach_stats {
    uint32_t total;
    uint32_t attached;
    uint32_t fallback;
    uint32_t missing;
    uint32_t isolated;
    uint32_t expected_test_failures;
};
void ghostattach_get_stats(struct ghostattach_stats *out);

#endif /* KMZ_GHOSTATTACH_H */
