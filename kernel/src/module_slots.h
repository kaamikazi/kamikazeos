/*
 * module_slots.h — ModuleSlots: the rollback-safe update layer.
 *
 * Real updaters never overwrite the running thing. They install into the
 * INACTIVE slot (A or B), switch the active pointer, verify, and keep the
 * old slot untouched as the guaranteed way back. v1.0 implements exactly
 * that flow at the METADATA level (honesty label: no binary module
 * loading exists yet — slots hold version strings, not code). When real
 * module images arrive, this metadata layer is already correct.
 *
 * Slot lifecycle for an update:
 *   apply   : inactive slot <- to_version, active <- inactive,
 *             rollback_slot <- old active, pending_verify = 1,
 *             new slot verified = 0
 *   verify  : new slot verified = 1, pending_verify = 0
 *   rollback: active <- rollback_slot (the untouched known-good slot)
 *
 * Persisted at LBA 56 (one sector, checksummed) so an applied-but-
 * unverified update is still pending after a reboot — or a power cut.
 */
#ifndef KMZ_MODULE_SLOTS_H
#define KMZ_MODULE_SLOTS_H

#include <stdint.h>
#include "module_versions.h"

#define SLOT_A 0
#define SLOT_B 1
#define SLOT_NONE 0xFF

#define SLOT_VER_MAX 8
#define SLOT_MODULE_COUNT 4     /* the updatable modules */

struct module_slot {
    enum module_id module;
    char    slot_a_version[SLOT_VER_MAX];
    char    slot_b_version[SLOT_VER_MAX];
    uint8_t active_slot;        /* SLOT_A or SLOT_B          */
    uint8_t slot_a_verified;
    uint8_t slot_b_verified;
    uint8_t rollback_slot;      /* SLOT_NONE if none         */
    uint8_t pending_verify;     /* applied but not verified  */
};

/* Load from disk or initialize defaults (slot A = base version,
   verified, active). Also syncs module_versions to active slots. */
void module_slots_init(void);

uint32_t                  module_slot_count(void);
const struct module_slot *module_slot_get(uint32_t index);
struct module_slot       *module_slot_for(enum module_id id); /* 0 if not updatable */

void module_slots_save(void);
void module_slots_clear(void);   /* back to defaults + save */

/* Convenience: the active slot's version string for a module. */
const char *module_slot_active_version(const struct module_slot *s);

/* 1 if ANY module has pending_verify set (for boot check + report). */
int module_slots_pending_verify(void);

#endif /* KMZ_MODULE_SLOTS_H */
