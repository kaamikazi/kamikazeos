/* recovery_store.c — disk-backed recovery state + append-only journal.
 * Read recovery_store.h first for the on-disk layout and design rules. */
#include "recovery_store.h"
#include "ata.h"
#include "errors.h"
#include "string.h"

/* ------------------------------------------------------------------ */
/* On-disk format constants (part of the format — change = new version) */
/* ------------------------------------------------------------------ */
#define LBA_STATE      1
#define LBA_JOURNAL    2
#define JOURNAL_SECTORS 14
#define ENTRY_SIZE     16
#define ENTRIES_PER_SECTOR (ATA_SECTOR_SIZE / ENTRY_SIZE)        /* 32  */
#define JOURNAL_CAPACITY   (JOURNAL_SECTORS * ENTRIES_PER_SECTOR)/* 448 */

/* State block: magic "KMZR", then version, then fields, then checksum. */
#define ST_MAGIC0 'K'
#define ST_MAGIC1 'M'
#define ST_MAGIC2 'Z'
#define ST_MAGIC3 'R'
#define ST_VERSION 1
/* byte offsets inside the state sector */
#define SB_MAGIC     0   /* 4 bytes  */
#define SB_VERSION   4
#define SB_BOOTCNT   5
#define SB_LASTCLEAN 6   /* boot number of the last clean boot          */
#define SB_PANIC     7
#define SB_SAFENEXT  8
#define SB_NETQ      9
#define SB_AUDQ      10
#define SB_RBAVAIL   11
#define SB_LASTACT   12
#define SB_PRIORNET  13
#define SB_PRIORAUD  14
#define SB_JCOUNT_LO 15  /* journal entry count, little-endian 16 bits  */
#define SB_JCOUNT_HI 16
#define SB_CHECKSUM  17  /* two's complement over bytes 0..16           */
#define SB_USED      18  /* bytes 18..511 stay zero (reserved)          */

/* Journal entry layout (16 bytes). */
#define JE_MAGIC_BYTE 0xA7
/* offsets: 0 magic, 1 version, 2 boot, 3 event, 4 action, 5 result,
            6 risk, 7 reversible, 8..13 short_code, 14 reserved, 15 csum */

static int      valid;            /* state block passed validation       */
static uint16_t jcount;           /* journal entries currently on disk   */
static uint8_t  sector[ATA_SECTOR_SIZE];   /* shared fixed I/O buffer    */

const char *journal_event_name(enum journal_event ev)
{
    switch (ev) {
    case JE_SUGGESTED:      return "SUGGESTED";
    case JE_APPROVED:       return "APPROVED";
    case JE_APPLIED:        return "APPLIED";
    case JE_ROLLED_BACK:    return "ROLLED_BACK";
    case JE_BLOCKED:        return "BLOCKED";
    case JE_PANIC:          return "PANIC";
    case JE_SAFE_MODE_BOOT: return "SAFE_MODE_BOOT";
    case JE_CLEAN_BOOT:     return "CLEAN_BOOT";
    case JE_PKG_SUGGESTED:   return "PKG-SUGGESTED";
    case JE_PKG_APPROVED:    return "PKG-APPROVED";
    case JE_PKG_APPLIED:     return "PKG-APPLIED";
    case JE_PKG_VERIFIED:    return "PKG-VERIFIED";
    case JE_PKG_FAILED:      return "PKG-FAILED";
    case JE_PKG_ROLLED_BACK: return "PKG-ROLLED_BACK";
    case JE_PKG_BLOCKED:     return "PKG-BLOCKED";
    }
    return "?";
}

/* ------------------------------------------------------------------ */
/* State block                                                          */
/* ------------------------------------------------------------------ */

static uint8_t checksum_over(const uint8_t *bytes, int n)
{
    uint8_t sum = 0;
    for (int i = 0; i < n; i++)
        sum = (uint8_t)(sum + bytes[i]);
    return (uint8_t)(0u - sum);     /* byte that makes the total 0 */
}

void recovery_store_init(void)
{
    valid  = 0;
    jcount = 0;

    ata_init();
    if (!ata_available())
        return;                     /* STORE-WARN already logged by ata  */

    if (!ata_read_sector(LBA_STATE, sector)) {
        error_log(SEV_WARNING, "STORE-WARN-003", "RecoveryStore",
                  "Could not read recovery state sector",
                  "durable store ignored - CMOS fallback in use");
        return;
    }

    /* Validate: magic, version, checksum. Fail any -> fresh store. */
    uint8_t sum = 0;
    for (int i = 0; i <= SB_CHECKSUM; i++)
        sum = (uint8_t)(sum + sector[i]);

    if (sector[SB_MAGIC]   != ST_MAGIC0 || sector[SB_MAGIC+1] != ST_MAGIC1 ||
        sector[SB_MAGIC+2] != ST_MAGIC2 || sector[SB_MAGIC+3] != ST_MAGIC3 ||
        sector[SB_VERSION] != ST_VERSION || sum != 0) {
        error_log(SEV_INFO, "STORE-001", "RecoveryStore",
                  "No valid recovery state on disk - starting fresh",
                  "normal for a new recovery image");
        return;
    }

    jcount = (uint16_t)(sector[SB_JCOUNT_LO] | (sector[SB_JCOUNT_HI] << 8));
    if (jcount > JOURNAL_CAPACITY)
        jcount = JOURNAL_CAPACITY;     /* never trust counts blindly */
    valid = 1;
    error_log(SEV_INFO, "STORE-002", "RecoveryStore",
              "Durable recovery state loaded from disk",
              "recovery now survives full emulator restarts");
}

int recovery_store_available(void) { return ata_available(); }
int recovery_store_valid(void)     { return valid; }

int recovery_store_load(struct recovery_memory *into)
{
    if (!valid)
        return 0;
    /* `sector` still holds the validated state block from init. */
    into->boot_count           = sector[SB_BOOTCNT];
    into->last_boot_panic      = sector[SB_PANIC];
    into->safe_mode_next_boot  = sector[SB_SAFENEXT];
    into->network_quarantined  = sector[SB_NETQ];
    into->audio_quarantined    = sector[SB_AUDQ];
    into->rollback_available   = sector[SB_RBAVAIL];
    into->last_action_id       = sector[SB_LASTACT];
    into->prior_network_status = sector[SB_PRIORNET];
    into->prior_audio_status   = sector[SB_PRIORAUD];
    return 1;
}

int recovery_store_save(const struct recovery_memory *from)
{
    if (!ata_available())
        return 0;

    kmemset(sector, 0, sizeof(sector));
    sector[SB_MAGIC]     = ST_MAGIC0;
    sector[SB_MAGIC + 1] = ST_MAGIC1;
    sector[SB_MAGIC + 2] = ST_MAGIC2;
    sector[SB_MAGIC + 3] = ST_MAGIC3;
    sector[SB_VERSION]   = ST_VERSION;
    sector[SB_BOOTCNT]   = from->boot_count;
    /* last_clean_boot: the boot number when the panic flag was last clear */
    sector[SB_LASTCLEAN] = from->last_boot_panic ? 0 : from->boot_count;
    sector[SB_PANIC]     = from->last_boot_panic;
    sector[SB_SAFENEXT]  = from->safe_mode_next_boot;
    sector[SB_NETQ]      = from->network_quarantined;
    sector[SB_AUDQ]      = from->audio_quarantined;
    sector[SB_RBAVAIL]   = from->rollback_available;
    sector[SB_LASTACT]   = from->last_action_id;
    sector[SB_PRIORNET]  = from->prior_network_status;
    sector[SB_PRIORAUD]  = from->prior_audio_status;
    sector[SB_JCOUNT_LO] = (uint8_t)(jcount & 0xFF);
    sector[SB_JCOUNT_HI] = (uint8_t)(jcount >> 8);
    sector[SB_CHECKSUM]  = checksum_over(sector, SB_CHECKSUM);

    if (!ata_write_sector(LBA_STATE, sector))
        return 0;
    valid = 1;
    return 1;
}

void recovery_store_clear(void)
{
    if (!ata_available())
        return;
    kmemset(sector, 0, sizeof(sector));
    ata_write_sector(LBA_STATE, sector);                 /* zero the state */
    for (int s = 0; s < JOURNAL_SECTORS; s++)            /* zero the log   */
        ata_write_sector((uint32_t)(LBA_JOURNAL + s), sector);
    valid  = 0;
    jcount = 0;
    error_log(SEV_INFO, "STORE-100", "RecoveryStore",
              "Durable recovery store cleared by user",
              "state block and journal zeroed");
}

/* ------------------------------------------------------------------ */
/* Append-only journal                                                  */
/* ------------------------------------------------------------------ */

void recovery_journal_append(enum journal_event ev, uint8_t action_id,
                             uint8_t result, uint8_t risk, uint8_t reversible,
                             const char *short_code)
{
    if (!ata_available())
        return;
    if (jcount >= JOURNAL_CAPACITY) {
        /* Append-only means FULL means STOP — history is never rewritten. */
        error_log(SEV_WARNING, "STORE-WARN-004", "RecoveryStore",
                  "Recovery journal is full - history preserved, appends stopped",
                  "clear-store confirm to start a fresh journal");
        return;
    }

    uint32_t lba    = LBA_JOURNAL + (jcount / ENTRIES_PER_SECTOR);
    uint32_t offset = (uint32_t)(jcount % ENTRIES_PER_SECTOR) * ENTRY_SIZE;

    /* Read-modify-write the sector this entry lands in.
       Failures are LOGGED (rule 5: silent failure is a bug) — but to
       serial only; error_log never re-enters the journal. */
    uint8_t buf[ATA_SECTOR_SIZE];
    if (!ata_read_sector(lba, buf)) {
        error_log(SEV_WARNING, "STORE-WARN-005", "RecoveryStore",
                  "Journal sector read failed - entry not recorded",
                  "event lost from durable journal");
        return;
    }

    uint8_t *e = buf + offset;
    kmemset(e, 0, ENTRY_SIZE);
    e[0] = JE_MAGIC_BYTE;
    e[1] = ST_VERSION;
    e[2] = recovery_memory_get()->boot_count;
    e[3] = (uint8_t)ev;
    e[4] = action_id;
    e[5] = result;
    e[6] = risk;
    e[7] = reversible;
    for (int i = 0; i < 6 && short_code[i] != '\0'; i++)
        e[8 + i] = (uint8_t)short_code[i];
    e[15] = checksum_over(e, 15);

    if (!ata_write_sector(lba, buf)) {
        error_log(SEV_WARNING, "STORE-WARN-006", "RecoveryStore",
                  "Journal sector write failed - entry not recorded",
                  "event lost from durable journal");
        return;
    }

    jcount++;
    /* Persist the new count (it lives in the state block). */
    recovery_store_save(recovery_memory_get());
}

uint32_t recovery_journal_count(void)
{
    return jcount;
}

int recovery_journal_get(uint32_t index, struct journal_entry *out)
{
    if (!ata_available() || index >= jcount)
        return 0;

    uint32_t lba    = LBA_JOURNAL + (index / ENTRIES_PER_SECTOR);
    uint32_t offset = (uint32_t)(index % ENTRIES_PER_SECTOR) * ENTRY_SIZE;

    uint8_t buf[ATA_SECTOR_SIZE];
    if (!ata_read_sector(lba, buf))
        return 0;

    const uint8_t *e = buf + offset;
    uint8_t sum = 0;
    for (int i = 0; i < ENTRY_SIZE; i++)
        sum = (uint8_t)(sum + e[i]);
    if (e[0] != JE_MAGIC_BYTE || sum != 0)
        return 0;                        /* corrupt entry: refuse it */

    out->boot_number = e[2];
    out->event_type  = e[3];
    out->action_id   = e[4];
    out->result      = e[5];
    out->risk_level  = e[6];
    out->reversible  = e[7];
    for (int i = 0; i < 6; i++)
        out->short_code[i] = (char)e[8 + i];
    out->short_code[6] = '\0';
    return 1;
}
