/*
 * recovery_store.h — RecoveryStore: durable long-term recovery memory.
 *
 * CMOS (RecoveryMemory's first home) dies with the QEMU process. The
 * RecoveryStore lives on a raw disk image, so it survives EVERYTHING
 * short of deleting the image file: shell reboots, QEMU resets, and full
 * QEMU close/restart.
 *
 * ON-DISK LAYOUT (fixed, no filesystem — deliberately):
 *   LBA 0      untouched (avoids any boot-sector/MBR confusion)
 *   LBA 1      recovery state block (one 512-byte sector)
 *   LBA 2..15  append-only journal: 14 sectors x 32 entries x 16 bytes
 *              = 448 entries of durable recovery history
 *
 * Both the state block and every journal entry carry magic + version +
 * checksum. Anything that fails validation is ignored safely — a fresh
 * or corrupted image just means "no durable memory yet", never a crash.
 *
 * The journal is APPEND-ONLY by design: recovery history is evidence,
 * and evidence that can be silently rewritten is worthless. When full,
 * appends stop (and say so) rather than overwrite history.
 */
#ifndef KMZ_RECOVERY_STORE_H
#define KMZ_RECOVERY_STORE_H

#include <stdint.h>
#include "recovery_memory.h"     /* struct recovery_memory */

/* Journal event types (stored as one byte — values are part of the
   on-disk format: never renumber, only append). */
enum journal_event {
    JE_SUGGESTED      = 1,
    JE_APPROVED       = 2,
    JE_APPLIED        = 3,
    JE_ROLLED_BACK    = 4,
    JE_BLOCKED        = 5,
    JE_PANIC          = 6,
    JE_SAFE_MODE_BOOT = 7,
    JE_CLEAN_BOOT     = 8,
    /* v0.9 package lifecycle (action_id field carries the package number) */
    JE_PKG_SUGGESTED   = 9,
    JE_PKG_APPROVED    = 10,
    JE_PKG_APPLIED     = 11,
    JE_PKG_VERIFIED    = 12,
    JE_PKG_FAILED      = 13,
    JE_PKG_ROLLED_BACK = 14,
    JE_PKG_BLOCKED     = 15,
};

/* One decoded journal entry (in-RAM view of the 16 on-disk bytes). */
struct journal_entry {
    uint8_t boot_number;
    uint8_t event_type;       /* enum journal_event                   */
    uint8_t action_id;        /* numeric: 1..4, 255 = ACT-999, 0=none */
    uint8_t result;           /* 1 = ok / 0 = refused or failed       */
    uint8_t risk_level;       /* enum risk_level                      */
    uint8_t reversible;       /* boolean, 0/1; on-disk reserved byte = 0 */
    char    short_code[7];    /* printable ASCII, zero-padded to 6 bytes */
};

/* Probe the disk and load+validate the state block. Call once at boot. */
void recovery_store_init(void);

int  recovery_store_available(void);   /* disk answered IDENTIFY        */
int  recovery_store_valid(void);       /* state block passed validation */

/* Hydrate `into` from the durable state block (only the persisted
   fields). Returns 1 if the store was valid and `into` was filled. */
int  recovery_store_load(struct recovery_memory *into);

/* Persist the given state to LBA 1. Returns 1 on success. */
int  recovery_store_save(const struct recovery_memory *from);

/* Wipe state block AND journal (writes zeroed sectors). */
void recovery_store_clear(void);

/* Append one entry to the durable journal. Safe no-op without a disk. */
void recovery_journal_append(enum journal_event ev, uint8_t action_id,
                             uint8_t result, uint8_t risk, uint8_t reversible,
                             const char *short_code);

uint32_t recovery_journal_count(void);
int      recovery_journal_get(uint32_t index, struct journal_entry *out);

const char *journal_event_name(enum journal_event ev);

/* v2.6.1 integrity view for the report and shell. */
struct recovery_store_integrity {
    const char *state_redundancy; /* mirror_lba50 / single_copy_capacity /
                                    unavailable                          */
    const char *state_source;   /* copy loaded at boot: primary / mirror /
                                   legacy_primary / fresh / damaged /
                                   ambiguous / unreadable / cleared       */
    uint32_t generation;        /* generation of the trusted state copy   */
    int state_locked;           /* state writes refused this boot         */
    int journal_locked;         /* journal appends refused this boot      */
    int journal_scan_valid;     /* all sectors read; boot evidence scanned */
    int journal_full;
    uint32_t journal_capacity;
    uint32_t dropped_appends;   /* appends refused (full/locked/failed)   */
    uint32_t corrupt_entries;   /* occupied slots failing validation      */
};
void recovery_store_integrity(struct recovery_store_integrity *out);

/* Event type (JE_SUGGESTED..JE_BLOCKED) of the most recent valid action
   lifecycle entry for this action id, or 0 if none / unreadable. Package
   events are not considered (their id byte is a package number). */
uint8_t recovery_journal_last_action_event(uint8_t action_id);

/* Highest boot number found in a valid journal entry (0 if none). Used to
   keep boot numbers moving forward when both state copies are lost. */
uint8_t recovery_store_max_journal_boot(void);

/* Diagnostic lower bound from readable validated state/journal records.
   Does not grant write permission when the store is locked. */
uint8_t recovery_store_boot_lower_bound(void);

/* Snapshot rotation needs consecutive history or a successful CLEAN_BOOT
   chain through a gap. Valid checksums do not authenticate that history. */
int recovery_journal_boot_progression_verified(uint8_t prior, uint8_t next);

#endif /* KMZ_RECOVERY_STORE_H */
