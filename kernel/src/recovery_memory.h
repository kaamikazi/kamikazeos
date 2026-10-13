/*
 * recovery_memory.h — RecoveryMemory: the OS's memory of what hurt it.
 *
 * Everything the RecoveryEngine did in v0.5 evaporated at reboot. This
 * module gives KamikazeOS a small, fixed-size persistent state block so
 * that quarantines, the safe-mode schedule, and panic evidence survive
 * a restart.
 *
 * WHERE IT LIVES — and an honest statement of how persistent it is:
 * the state is stored in the PC's CMOS/RTC NVRAM (the battery-backed
 * 128-byte memory of the real-time clock chip, reached through I/O ports
 * 0x70/0x71 — the same place the BIOS keeps its settings). This is REAL
 * hardware persistence: it survives reboots, kernel panics, and triple
 * faults. LIMIT: QEMU re-creates CMOS contents when the QEMU *process*
 * itself is restarted (there is no backing file by default), so state
 * survives in-VM reboots but not closing the emulator. Disk-backed
 * persistence (which survives everything) is the Phase 7 upgrade — only
 * this module's load/save internals change; the API stays.
 *
 * SAFETY RULES OF THIS MODULE:
 *  - fixed 9-byte layout at CMOS offsets 0x60..0x68, nothing else touched
 *  - 2 magic bytes + version + checksum: corrupt/foreign data is treated
 *    as "no memory" and re-initialized, never trusted
 *  - fixed bytes only — no strings, no pointers, no variable lengths
 */
#ifndef KMZ_RECOVERY_MEMORY_H
#define KMZ_RECOVERY_MEMORY_H

#include <stdint.h>
#include "device.h"     /* enum device_status (persisted as one byte) */

/* The in-RAM mirror of the persistent block (plus runtime-only fields). */
struct recovery_memory {
    /* persisted fields */
    uint8_t safe_mode_next_boot;    /* boot into safe mode next time      */
    uint8_t network_quarantined;    /* ACT-001 is in effect               */
    uint8_t audio_quarantined;      /* ACT-002 is in effect               */
    uint8_t last_boot_panic;        /* previous boot ended in kpanic()    */
    uint8_t boot_count;             /* wraps at 255 — fine for a counter  */
    uint8_t last_action_id;         /* numeric: 1..4, 0 = none            */
    uint8_t rollback_available;     /* any reversible action is applied   */
    uint8_t prior_network_status;   /* enum device_status before ACT-001  */
    uint8_t prior_audio_status;     /* enum device_status before ACT-002  */
    uint8_t last_panic_intentional; /* v2.6.1: that panic was a deliberate */
                                    /* test (CMOS flag bit 5 / disk ext)   */

    /* runtime-only fields (never persisted) */
    uint8_t safe_mode_active;       /* THIS boot is a safe-mode boot      */
    uint8_t prev_panic_detected;    /* panic flag seen at load time —     */
                                    /* kept for the whole session even    */
                                    /* after the stored flag is cleared   */
    uint8_t valid_at_boot;          /* magic+checksum matched at load     */
    uint8_t prev_panic_intentional; /* runtime copy of the above at load  */
};

void recovery_memory_init(void);    /* load + boot_count++ + save         */
void recovery_memory_load(void);
void recovery_memory_save(void);
void recovery_memory_clear(void);   /* zero everything, write fresh block */
void recovery_memory_print(void);   /* human view, used by `memory` cmd   */

void recovery_memory_set_safe_mode_next_boot(int enabled);
void recovery_memory_mark_action_applied(const char *action_id,
                                         enum device_status prior);
void recovery_memory_mark_action_rolled_back(const char *action_id);
void recovery_memory_mark_panic(int intentional); /* called from kpanic() */
void recovery_memory_mark_clean_boot(void);  /* clears panic flag — call  */
                                             /* only AFTER it is reported */

/* Read-only view for kernel.c / shell / report. */
const struct recovery_memory *recovery_memory_get(void);

/* v0.7: which persistence backend hydrated this boot's state.
   "disk+CMOS" / "CMOS_only" / "fresh" — shown in UI and reports. */
const char *recovery_memory_backend(void);
int         recovery_memory_used_cmos_fallback(void);  /* disk bad, CMOS used */

/* Set/read the runtime "this boot is safe mode" flag. */
void recovery_memory_set_safe_mode_active(int active);

#endif /* KMZ_RECOVERY_MEMORY_H */
