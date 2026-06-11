/*
 * boot_snapshot.h — BootSnapshot: the OS's health memory across boots.
 *
 * The journal (recovery_store) remembers EVENTS. Snapshots remember
 * STATES: one fixed-size record per boot capturing how healthy the
 * machine was — device counts, error counts, safe mode, panic evidence,
 * and a single deterministic 0-100 HEALTH SCORE. With states on disk,
 * the analyzer can finally answer time questions: did safe mode help?
 * did the rollback restore what we expected? is this boot worse than
 * the last one?
 *
 * THE HEALTH SCORE (simple and deterministic on purpose — a score you
 * can't recompute by hand is a score you can't trust):
 *     start at 100
 *     -15  if the previous boot ended in panic
 *     -10  per device with no driver (missing_driver)
 *     - 8  per isolated device
 *     - 5  per warning logged this boot
 *     -25  per critical (error/panic) logged this boot
 *     floor at 0
 *
 * ON-DISK: LBA 16..31 of the recovery image, ONE SNAPSHOT PER SECTOR,
 * slot = boot_number % 16 (a 16-boot ring). Each snapshot has magic,
 * version, and checksum; invalid slots are skipped on read, never
 * trusted. No disk -> snapshots silently unavailable, boot continues.
 */
#ifndef KMZ_BOOT_SNAPSHOT_H
#define KMZ_BOOT_SNAPSHOT_H

#include <stdint.h>

#define SNAPSHOT_SLOTS 16

/* In-RAM view of one snapshot record. */
struct boot_snapshot {
    uint8_t boot_number;
    uint8_t safe_mode_active;
    uint8_t prev_panic_detected;
    uint8_t total_devices;
    uint8_t attached_count;
    uint8_t fallback_count;
    uint8_t missing_count;
    uint8_t isolated_count;
    uint8_t blocked_driver_count;
    uint8_t warning_count;
    uint8_t critical_count;
    uint8_t actions_applied;
    uint8_t rollback_available;
    uint8_t scan_ok;            /* 1 = every device attached or fallback */
    uint8_t health_score;       /* 0..100 */
};

/* Why the current score is what it is (for the `health` command). */
struct health_breakdown {
    int base;                   /* always 100             */
    int panic_penalty;          /* 0 or -15               */
    int missing_penalty;        /* -10 * missing          */
    int isolated_penalty;       /* -8  * isolated         */
    int warning_penalty;        /* -5  * warnings         */
    int critical_penalty;       /* -25 * criticals        */
    int score;                  /* clamped result         */
};

/* Compute the CURRENT boot's health from live system state. */
void boot_snapshot_health_now(struct health_breakdown *out);

/* Capture current state and persist it to this boot's slot.
   Logs SNAP-WARN-001 and returns 0 if no disk is available. */
int boot_snapshot_save(void);

/* Read all valid snapshots, sorted oldest-first by boot number.
   `out` must hold SNAPSHOT_SLOTS entries. Returns how many are valid. */
int boot_snapshot_list(struct boot_snapshot *out);

#endif /* KMZ_BOOT_SNAPSHOT_H */
