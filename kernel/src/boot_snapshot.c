/* boot_snapshot.c — capture, persist, and read per-boot health records.
 * Read boot_snapshot.h first for the design and the health formula. */
#include "boot_snapshot.h"
#include "ata.h"
#include "errors.h"
#include "ghostattach.h"
#include "recovery_actions.h"
#include "recovery_memory.h"
#include "string.h"

/* On-disk layout constants. */
#define LBA_SNAPSHOTS 16
#define SN_MAGIC0 'K'
#define SN_MAGIC1 'S'
#define SN_MAGIC2 'N'
#define SN_MAGIC3 'P'
#define SN_VERSION 1
/* byte offsets in the sector */
#define SN_OFF_MAGIC    0    /* 4 bytes                  */
#define SN_OFF_VERSION  4
#define SN_OFF_BOOT     5
#define SN_OFF_FLAGS    6    /* bit0 safe, bit1 panic, bit2 rb, bit3 scan_ok */
#define SN_OFF_TOTAL    7
#define SN_OFF_ATTACH   8
#define SN_OFF_FALLBACK 9
#define SN_OFF_MISSING  10
#define SN_OFF_ISOLATED 11
#define SN_OFF_BLOCKED  12
#define SN_OFF_WARN     13
#define SN_OFF_CRIT     14
#define SN_OFF_APPLIED  15
#define SN_OFF_HEALTH   16
#define SN_OFF_CHECKSUM 17

/* Count capped into one byte (the log ring can't exceed 64 anyway). */
static uint8_t cap8(uint32_t v)
{
    return (v > 255) ? 255 : (uint8_t)v;
}

void boot_snapshot_health_now(struct health_breakdown *out)
{
    struct ghostattach_stats st;
    ghostattach_get_stats(&st);

    uint32_t warnings  = error_count_severity(SEV_WARNING);
    uint32_t criticals = error_count_severity(SEV_ERROR) +
                         error_count_severity(SEV_PANIC);
    uint32_t production_isolated = st.isolated >= st.expected_test_failures
                                 ? st.isolated - st.expected_test_failures : 0;

    out->base             = 100;
    out->panic_penalty    = recovery_memory_get()->prev_panic_detected ? -15 : 0;
    out->missing_penalty  = -10 * (int)st.missing;
    out->isolated_penalty = -8  * (int)production_isolated;
    out->warning_penalty  = -5  * (int)warnings;
    out->critical_penalty = -25 * (int)criticals;

    int score = out->base + out->panic_penalty + out->missing_penalty +
                out->isolated_penalty + out->warning_penalty +
                out->critical_penalty;
    out->score = (score < 0) ? 0 : score;
}

/* Gather everything the snapshot records from the live system. */
static void capture(struct boot_snapshot *s)
{
    const struct recovery_memory *m = recovery_memory_get();
    struct ghostattach_stats st;
    ghostattach_get_stats(&st);

    struct health_breakdown hb;
    boot_snapshot_health_now(&hb);

    /* Count blocked drivers and applied actions from the registries. */
    uint8_t blocked = 0;
    for (uint32_t i = 0; i < ghostattach_driver_count(); i++)
        if (ghostattach_driver(i)->status == DRVST_BLOCKED_UNVERIFIED)
            blocked++;
    uint8_t applied = 0;
    for (uint32_t i = 0; i < recovery_action_count(); i++)
        if (recovery_action(i)->status == ACTST_APPLIED)
            applied++;

    s->boot_number          = m->boot_count;
    s->safe_mode_active     = m->safe_mode_active;
    s->prev_panic_detected  = m->prev_panic_detected;
    s->total_devices        = cap8(st.total);
    s->attached_count       = cap8(st.attached);
    s->fallback_count       = cap8(st.fallback);
    s->missing_count        = cap8(st.missing);
    s->isolated_count       = cap8(st.isolated >= st.expected_test_failures
                                 ? st.isolated - st.expected_test_failures : 0);
    s->blocked_driver_count = blocked;
    s->warning_count        = cap8(error_count_severity(SEV_WARNING));
    s->critical_count       = cap8(error_count_severity(SEV_ERROR) +
                                   error_count_severity(SEV_PANIC));
    s->actions_applied      = applied;
    s->rollback_available   = m->rollback_available;
    s->scan_ok              = (st.attached + st.fallback == st.total) ? 1 : 0;
    s->health_score         = (uint8_t)hb.score;
}

int boot_snapshot_save(void)
{
    if (!ata_available()) {
        error_log(SEV_WARNING, "SNAP-WARN-001", "BootSnapshot",
                  "No disk - boot snapshot not persisted",
                  "attach build/recovery.img for health history");
        return 0;
    }

    struct boot_snapshot s;
    capture(&s);

    uint8_t sec[ATA_SECTOR_SIZE];
    kmemset(sec, 0, sizeof(sec));
    sec[SN_OFF_MAGIC]     = SN_MAGIC0;
    sec[SN_OFF_MAGIC + 1] = SN_MAGIC1;
    sec[SN_OFF_MAGIC + 2] = SN_MAGIC2;
    sec[SN_OFF_MAGIC + 3] = SN_MAGIC3;
    sec[SN_OFF_VERSION]   = SN_VERSION;
    sec[SN_OFF_BOOT]      = s.boot_number;
    sec[SN_OFF_FLAGS]     = (uint8_t)((s.safe_mode_active    ? 1 : 0) |
                                      (s.prev_panic_detected ? 2 : 0) |
                                      (s.rollback_available  ? 4 : 0) |
                                      (s.scan_ok             ? 8 : 0));
    sec[SN_OFF_TOTAL]     = s.total_devices;
    sec[SN_OFF_ATTACH]    = s.attached_count;
    sec[SN_OFF_FALLBACK]  = s.fallback_count;
    sec[SN_OFF_MISSING]   = s.missing_count;
    sec[SN_OFF_ISOLATED]  = s.isolated_count;
    sec[SN_OFF_BLOCKED]   = s.blocked_driver_count;
    sec[SN_OFF_WARN]      = s.warning_count;
    sec[SN_OFF_CRIT]      = s.critical_count;
    sec[SN_OFF_APPLIED]   = s.actions_applied;
    sec[SN_OFF_HEALTH]    = s.health_score;

    uint8_t sum = 0;
    for (int i = 0; i < SN_OFF_CHECKSUM; i++)
        sum = (uint8_t)(sum + sec[i]);
    sec[SN_OFF_CHECKSUM] = (uint8_t)(0u - sum);

    /* One slot per boot, ring of 16: old history ages out naturally. */
    uint32_t lba = LBA_SNAPSHOTS + (s.boot_number % SNAPSHOT_SLOTS);
    return ata_write_sector(lba, sec);
}

/* Decode + validate one slot. Returns 1 if it held a real snapshot. */
static int read_slot(uint32_t slot, struct boot_snapshot *out)
{
    uint8_t sec[ATA_SECTOR_SIZE];
    if (!ata_read_sector(LBA_SNAPSHOTS + slot, sec))
        return 0;

    uint8_t sum = 0;
    for (int i = 0; i <= SN_OFF_CHECKSUM; i++)
        sum = (uint8_t)(sum + sec[i]);
    if (sec[SN_OFF_MAGIC]     != SN_MAGIC0 || sec[SN_OFF_MAGIC + 1] != SN_MAGIC1 ||
        sec[SN_OFF_MAGIC + 2] != SN_MAGIC2 || sec[SN_OFF_MAGIC + 3] != SN_MAGIC3 ||
        sec[SN_OFF_VERSION]   != SN_VERSION || sum != 0)
        return 0;                          /* empty or corrupt: skip */

    out->boot_number          = sec[SN_OFF_BOOT];
    out->safe_mode_active     = (sec[SN_OFF_FLAGS] & 1) ? 1 : 0;
    out->prev_panic_detected  = (sec[SN_OFF_FLAGS] & 2) ? 1 : 0;
    out->rollback_available   = (sec[SN_OFF_FLAGS] & 4) ? 1 : 0;
    out->scan_ok              = (sec[SN_OFF_FLAGS] & 8) ? 1 : 0;
    out->total_devices        = sec[SN_OFF_TOTAL];
    out->attached_count       = sec[SN_OFF_ATTACH];
    out->fallback_count       = sec[SN_OFF_FALLBACK];
    out->missing_count        = sec[SN_OFF_MISSING];
    out->isolated_count       = sec[SN_OFF_ISOLATED];
    out->blocked_driver_count = sec[SN_OFF_BLOCKED];
    out->warning_count        = sec[SN_OFF_WARN];
    out->critical_count       = sec[SN_OFF_CRIT];
    out->actions_applied      = sec[SN_OFF_APPLIED];
    out->health_score         = sec[SN_OFF_HEALTH];
    return 1;
}

int boot_snapshot_list(struct boot_snapshot *out)
{
    if (!ata_available())
        return 0;

    int n = 0;
    for (uint32_t slot = 0; slot < SNAPSHOT_SLOTS; slot++)
        if (read_slot(slot, &out[n]))
            n++;

    /* Insertion-sort by boot number, oldest first (n <= 16: trivial). */
    for (int i = 1; i < n; i++) {
        struct boot_snapshot key = out[i];
        int j = i - 1;
        while (j >= 0 && out[j].boot_number > key.boot_number) {
            out[j + 1] = out[j];
            j--;
        }
        out[j + 1] = key;
    }
    return n;
}
