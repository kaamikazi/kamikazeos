/* recovery_store.c — disk-backed recovery state + append-only journal.
 * Read recovery_store.h first for the on-disk layout and design rules. */
#include "recovery_store.h"
#include "ata.h"
#include "errors.h"
#include "generation.h"
#include "string.h"

/* ------------------------------------------------------------------ */
/* On-disk format constants (part of the format — change = new version) */
/* ------------------------------------------------------------------ */
#define LBA_STATE        1
#define LBA_STATE_MIRROR 50   /* v2.6.1: full copy of the LBA 1 sector     */
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

/*
 * v2.6.1 generation extension. It lives in bytes that v1 kernels always
 * wrote as zero and never read, so LBA 1 stays a valid v1 block: older
 * kernels load it unchanged (and simply zero the extension when they
 * save). The CRC-32 covers the v1 fields AND the extension.
 */
#define EXT_OFF      32
#define EXT_MAGIC    "KSX1"
#define EXT_VERSION  (EXT_OFF + 4)
#define EXT_FLAGS    (EXT_OFF + 5)
#define EXT_GEN      (EXT_OFF + 8)   /* uint32 LE, +1 per save            */
#define EXT_JCOUNT   (EXT_OFF + 12)  /* uint16 LE copy of SB_JCOUNT       */
#define EXT_CRC      48              /* CRC-32 (IEEE) over bytes 0..47    */
#define EXT_FMT      1
#define EXT_FLAG_PANIC_INTENTIONAL 0x01   /* v2.6.1 health classification */

/* Journal entry layout (16 bytes). */
#define JE_MAGIC_BYTE 0xA7
/* offsets: 0 magic, 1 version, 2 boot, 3 event, 4 action, 5 result,
            6 risk, 7 reversible, 8..13 short_code, 14 reserved, 15 csum */

static int      valid;            /* a trusted state copy was loaded     */
static uint16_t jcount;           /* next journal slot (append position) */
static uint32_t generation;       /* generation of the trusted copy      */
static uint8_t  ext_flags;        /* extension flags of the loaded copy  */
static int      state_locked;     /* ambiguous/unreadable: never write   */
static int      journal_locked;   /* ambiguous/unreadable: never append  */
static int      full_warned;      /* STORE-WARN-004 logged this boot     */
static int      locked_warned;
static int      redundancy_warned;
static int      mirror_present;   /* capacity proves LBA 50 exists       */
static int      journal_scan_valid;
static uint32_t dropped;          /* appends refused this boot           */
static uint32_t corrupt_slots;    /* occupied slots failing validation   */
static uint8_t  max_journal_boot; /* highest boot number in valid entries */
static uint8_t  boot_lower_bound; /* validated state/journal evidence     */
static const char *state_source = "none";
static uint8_t  loaded[ATA_SECTOR_SIZE];   /* the trusted state copy     */
static uint8_t  sector[ATA_SECTOR_SIZE];   /* scratch I/O buffer         */

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
/* Encoding helpers                                                     */
/* ------------------------------------------------------------------ */

static uint8_t checksum_over(const uint8_t *bytes, int n)
{
    uint8_t sum = 0;
    for (int i = 0; i < n; i++)
        sum = (uint8_t)(sum + bytes[i]);
    return (uint8_t)(0u - sum);     /* byte that makes the total 0 */
}

static uint32_t crc32(const uint8_t *data, uint32_t length)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

static uint32_t get_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int all_zero(const uint8_t *p, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++)
        if (p[i])
            return 0;
    return 1;
}

/* One validity rule for ordering, reports, and lifecycle lookups. Occupancy
   is deliberately separate: invalid nonzero entries still reserve their slot. */
static int journal_entry_valid(const uint8_t *e)
{
    uint8_t sum = 0;
    for (int i = 0; i < ENTRY_SIZE; i++)
        sum = (uint8_t)(sum + e[i]);
    return e[0] == JE_MAGIC_BYTE && e[1] == ST_VERSION && sum == 0 &&
           e[3] >= JE_SUGGESTED && e[3] <= JE_PKG_BLOCKED;
}

static int v1_valid(const uint8_t *s)
{
    uint8_t sum = 0;
    for (int i = 0; i <= SB_CHECKSUM; i++)
        sum = (uint8_t)(sum + s[i]);
    return s[SB_MAGIC] == ST_MAGIC0 && s[SB_MAGIC + 1] == ST_MAGIC1 &&
           s[SB_MAGIC + 2] == ST_MAGIC2 && s[SB_MAGIC + 3] == ST_MAGIC3 &&
           s[SB_VERSION] == ST_VERSION && sum == 0;
}

static int ext_magic_present(const uint8_t *s)
{
    return s[EXT_OFF] == 'K' && s[EXT_OFF + 1] == 'S' &&
           s[EXT_OFF + 2] == 'X' && s[EXT_OFF + 3] == '1';
}

static int ext_valid(const uint8_t *s)
{
    return v1_valid(s) && ext_magic_present(s) && s[EXT_VERSION] == EXT_FMT &&
           get_u32(s + EXT_CRC) == crc32(s, EXT_CRC);
}

static int sectors_equal(const uint8_t *a, const uint8_t *b)
{
    for (int i = 0; i < ATA_SECTOR_SIZE; i++)
        if (a[i] != b[i])
            return 0;
    return 1;
}

/* ------------------------------------------------------------------ */
/* Init: choose a trusted state copy, then rebuild the journal position */
/* ------------------------------------------------------------------ */

enum state_choice { CHOICE_NONE, CHOICE_PRIMARY, CHOICE_MIRROR, CHOICE_AMBIGUOUS };

static enum state_choice choose_state(const uint8_t *p, const uint8_t *m,
                                      int *primary_damaged)
{
    int pe = ext_valid(p);
    int me = ext_valid(m);
    /* Legacy writers zeroed the entire reserved tail. Missing extension
       magic alone is insufficient: damaged modern magic must not bypass
       CRC validation and outvote a valid mirror through the v1 checksum. */
    int p_legacy = v1_valid(p) &&
                   all_zero(p + SB_CHECKSUM + 1,
                            ATA_SECTOR_SIZE - SB_CHECKSUM - 1);
    *primary_damaged = !pe && !p_legacy && !all_zero(p, ATA_SECTOR_SIZE);

    if (pe && me) {
        uint32_t gp = get_u32(p + EXT_GEN);
        uint32_t gm = get_u32(m + EXT_GEN);
        enum generation_order order = generation_compare(gm, gp);
        if (order == GENERATION_AMBIGUOUS)
            return CHOICE_AMBIGUOUS;
        if (order == GENERATION_EQUAL)
            return sectors_equal(p, m) ? CHOICE_PRIMARY : CHOICE_AMBIGUOUS;
        return order == GENERATION_NEWER ? CHOICE_MIRROR : CHOICE_PRIMARY;
    }
    if (pe)
        return CHOICE_PRIMARY;
    /* A legacy (pre-2.6.1) writer is always the most recent writer of a
       primary without the extension: newer kernels always write it. */
    if (p_legacy)
        return CHOICE_PRIMARY;
    if (me)
        return CHOICE_MIRROR;
    return CHOICE_NONE;
}

/*
 * Rebuild the append position from the journal itself. The position is
 * one past the LAST occupied (non-zero) slot, so no byte of existing
 * evidence — valid, torn, or corrupt — is ever overwritten. Returns 0 if
 * any journal sector is unreadable (position unknown).
 */
static int scan_journal(uint32_t *last_used, uint32_t *corrupt, uint8_t *max_boot)
{
    *last_used = 0;
    *corrupt = 0;
    *max_boot = 0;
    int complete = 1;
    for (uint32_t s = 0; s < JOURNAL_SECTORS; s++) {
        if (!ata_read_sector(LBA_JOURNAL + s, sector)) {
            complete = 0;
            continue; /* Readable later entries still establish a lower bound. */
        }
        for (uint32_t i = 0; i < ENTRIES_PER_SECTOR; i++) {
            const uint8_t *e = sector + i * ENTRY_SIZE;
            if (all_zero(e, ENTRY_SIZE))
                continue;
            *last_used = s * ENTRIES_PER_SECTOR + i + 1;
            if (!journal_entry_valid(e)) {
                (*corrupt)++;
                continue;
            }
            if (e[2] > *max_boot)
                *max_boot = e[2];
        }
    }
    return complete;
}

static void lock_store(const char *code, const char *message)
{
    state_locked = 1;
    journal_locked = 1;
    valid = 0;
    error_log(SEV_ERROR, code, "RecoveryStore", message,
              "store read-only this boot - inspect disk before clear-store");
}

void recovery_store_init(void)
{
    valid = 0;
    jcount = 0;
    generation = 0;
    ext_flags = 0;
    state_locked = journal_locked = 0;
    full_warned = locked_warned = redundancy_warned = 0;
    dropped = corrupt_slots = 0;
    max_journal_boot = 0;
    boot_lower_bound = 0;
    state_source = "none";
    mirror_present = 0;
    journal_scan_valid = 0;
    kmemset(loaded, 0, sizeof(loaded));

    ata_init();
    if (!ata_available())
        return;                     /* STORE-WARN already logged by ata  */

    mirror_present = ata_capacity_sectors() > LBA_STATE_MIRROR;

    static uint8_t primary[ATA_SECTOR_SIZE];
    static uint8_t mirror[ATA_SECTOR_SIZE];
    kmemset(mirror, 0, sizeof(mirror));
    int primary_read = ata_read_sector(LBA_STATE, primary);
    int mirror_read = !mirror_present || ata_read_sector(LBA_STATE_MIRROR, mirror);
    uint32_t last_used = 0;
    journal_scan_valid = scan_journal(&last_used, &corrupt_slots, &max_journal_boot);
    boot_lower_bound = max_journal_boot;
    jcount = (uint16_t)last_used;
    if (!primary_read || !mirror_read) {
        state_source = "unreadable";
        lock_store("STORE-ERR-012", "Recovery state sector unreadable - position unknown");
        return;
    }

    int primary_damaged = 0;
    enum state_choice choice = choose_state(primary, mirror, &primary_damaged);
    if (choice == CHOICE_AMBIGUOUS) {
        state_source = "ambiguous";
        lock_store("STORE-ERR-011",
                   "State copy generation ordering is ambiguous");
        return;
    }

    uint16_t recorded = 0;
    if (choice != CHOICE_NONE) {
        memcpy(loaded, choice == CHOICE_MIRROR ? mirror : primary, sizeof(loaded));
        valid = 1;
        if (loaded[SB_BOOTCNT] > boot_lower_bound)
            boot_lower_bound = loaded[SB_BOOTCNT];
        recorded = (uint16_t)(loaded[SB_JCOUNT_LO] | (loaded[SB_JCOUNT_HI] << 8));
        if (ext_valid(loaded)) {
            generation = get_u32(loaded + EXT_GEN);
            ext_flags = loaded[EXT_FLAGS];
            state_source = choice == CHOICE_MIRROR ? "mirror" : "primary";
        } else {
            /* The legacy primary supplies flags, but an older-format
               writer did not invalidate the mirror's generation floor. */
            if (ext_valid(mirror))
                generation = get_u32(mirror + EXT_GEN);
            state_source = "legacy_primary";
        }
        if (choice == CHOICE_MIRROR)
            error_log(SEV_WARNING, "STORE-WARN-009", "RecoveryStore",
                      "Primary recovery state damaged - mirror copy loaded",
                      "next save rewrites both copies from the mirror");
        else
            error_log(SEV_INFO, "STORE-002", "RecoveryStore",
                      "Durable recovery state loaded from disk",
                      "recovery now survives full emulator restarts");
    } else if (primary_damaged || !all_zero(mirror, ATA_SECTOR_SIZE)) {
        state_source = "damaged";
        error_log(SEV_ERROR, "STORE-ERR-010", "RecoveryStore",
                  "Both recovery state copies damaged",
                  "flags from CMOS - journal position rebuilt from entries");
    } else {
        state_source = "fresh";
        error_log(SEV_INFO, "STORE-001", "RecoveryStore",
                  "No valid recovery state on disk - starting fresh",
                  "normal for a new recovery image");
    }

    if (!journal_scan_valid) {
        if (recorded > jcount)
            jcount = recorded > JOURNAL_CAPACITY ? JOURNAL_CAPACITY : recorded;
        lock_store("STORE-ERR-012", "Journal sector unreadable - append position unknown");
        return;
    }

    if (max_journal_boot == 255 || (valid && loaded[SB_BOOTCNT] == 255)) {
        lock_store("STORE-ERR-015", "Boot number exhausted - persistent ordering held");
        return;
    }

    uint32_t position = last_used;
    if (valid && recorded > position) {
        /* The state says more entries exist than the disk shows. Never
           hand those slots out again: they may be someone's evidence. */
        position = recorded > JOURNAL_CAPACITY ? JOURNAL_CAPACITY : recorded;
        error_log(SEV_WARNING, "STORE-WARN-010", "RecoveryStore",
                  "Journal has fewer entries than recorded",
                  "claimed slots stay reserved - nothing reused");
    } else if (position > recorded) {
        error_log(SEV_WARNING, "STORE-WARN-008", "RecoveryStore",
                  "Journal position rebuilt from validated entries",
                  "torn append or lost state - existing entries preserved");
    }
    if (corrupt_slots)
        error_log(SEV_WARNING, "STORE-WARN-011", "RecoveryStore",
                  "Corrupt journal entries preserved in place",
                  "entries fail checksum - kept as evidence never reused");
    jcount = (uint16_t)position;
}

int recovery_store_available(void) { return ata_available(); }
int recovery_store_valid(void)     { return valid; }

int recovery_store_load(struct recovery_memory *into)
{
    if (!valid)
        return 0;
    into->boot_count           = loaded[SB_BOOTCNT];
    into->last_boot_panic      = loaded[SB_PANIC];
    into->safe_mode_next_boot  = loaded[SB_SAFENEXT];
    into->network_quarantined  = loaded[SB_NETQ];
    into->audio_quarantined    = loaded[SB_AUDQ];
    into->rollback_available   = loaded[SB_RBAVAIL];
    into->last_action_id       = loaded[SB_LASTACT];
    into->prior_network_status = loaded[SB_PRIORNET];
    into->prior_audio_status   = loaded[SB_PRIORAUD];
    /* Legacy blocks have no extension: their panics count as real. */
    into->last_panic_intentional = (loaded[SB_PANIC] &&
                                    (ext_flags & EXT_FLAG_PANIC_INTENTIONAL)) ? 1 : 0;
    return 1;
}

int recovery_store_save(const struct recovery_memory *from)
{
    if (!ata_available() || state_locked)
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

    uint32_t next_generation = generation + 1u;
    sector[EXT_OFF]     = 'K'; sector[EXT_OFF + 1] = 'S';
    sector[EXT_OFF + 2] = 'X'; sector[EXT_OFF + 3] = '1';
    sector[EXT_VERSION] = EXT_FMT;
    ext_flags = (from->last_boot_panic && from->last_panic_intentional)
                    ? EXT_FLAG_PANIC_INTENTIONAL : 0;
    sector[EXT_FLAGS]   = ext_flags;
    put_u32(sector + EXT_GEN, next_generation);
    sector[EXT_JCOUNT]     = (uint8_t)(jcount & 0xFF);
    sector[EXT_JCOUNT + 1] = (uint8_t)(jcount >> 8);
    put_u32(sector + EXT_CRC, crc32(sector, EXT_CRC));

    /* Mirror first: a crash between the two writes leaves a NEWER mirror,
       which init selects; a torn primary fails its CRC and loses too. */
    int mirror_ok  = mirror_present && ata_write_sector(LBA_STATE_MIRROR, sector);
    int primary_ok = ata_write_sector(LBA_STATE, sector);
    if (!mirror_ok && !primary_ok) {
        lock_store("STORE-ERR-014", "Recovery state write failed - persistence held");
        return 0;
    }
    if (mirror_present && (!mirror_ok || !primary_ok) && !redundancy_warned) {
        redundancy_warned = 1;
        error_log(SEV_WARNING, "STORE-WARN-012", "RecoveryStore",
                  "Only one recovery state copy was written",
                  "state is durable but redundancy is degraded");
    }
    generation = next_generation;
    memcpy(loaded, sector, sizeof(loaded));
    valid = 1;
    return 1;
}

void recovery_store_clear(void)
{
    if (!ata_available())
        return;
    kmemset(sector, 0, sizeof(sector));
    ata_write_sector(LBA_STATE, sector);                 /* zero the state */
    if (mirror_present)
        ata_write_sector(LBA_STATE_MIRROR, sector);
    for (int s = 0; s < JOURNAL_SECTORS; s++)            /* zero the log   */
        ata_write_sector((uint32_t)(LBA_JOURNAL + s), sector);
    valid  = 0;
    jcount = 0;
    generation = 0;
    ext_flags = 0;
    state_locked = journal_locked = 0;
    full_warned = locked_warned = 0;
    corrupt_slots = 0;
    max_journal_boot = 0;
    state_source = "cleared";
    error_log(SEV_INFO, "STORE-100", "RecoveryStore",
              "Durable recovery store cleared by user",
              "state blocks and journal zeroed");
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
    if (journal_locked) {
        dropped++;
        if (!locked_warned) {
            locked_warned = 1;
            error_log(SEV_WARNING, "STORE-WARN-013", "RecoveryStore",
                      "Journal append refused while store is read-only",
                      "events stay in the serial log only this boot");
        }
        return;
    }
    if (jcount >= JOURNAL_CAPACITY) {
        /* Append-only means FULL means STOP — history is never rewritten.
           Say so once per boot; count every refused append. */
        dropped++;
        if (!full_warned) {
            full_warned = 1;
            error_log(SEV_WARNING, "STORE-WARN-004", "RecoveryStore",
                      "Recovery journal is full - history preserved, appends stopped",
                      "clear-store confirm to start a fresh journal");
        }
        return;
    }

    uint32_t lba    = LBA_JOURNAL + (jcount / ENTRIES_PER_SECTOR);
    uint32_t offset = (uint32_t)(jcount % ENTRIES_PER_SECTOR) * ENTRY_SIZE;

    /* Read-modify-write the sector this entry lands in.
       Failures are LOGGED (rule 5: silent failure is a bug) — but to
       serial only; error_log never re-enters the journal. */
    uint8_t buf[ATA_SECTOR_SIZE];
    if (!ata_read_sector(lba, buf)) {
        dropped++;
        error_log(SEV_WARNING, "STORE-WARN-005", "RecoveryStore",
                  "Journal sector read failed - entry not recorded",
                  "event lost from durable journal");
        return;
    }

    uint8_t *e = buf + offset;
    if (!all_zero(e, ENTRY_SIZE)) {
        /* The scan placed the position after every occupied slot, so this
           means something wrote behind our back. Stop, don't overwrite. */
        dropped++;
        journal_locked = 1;
        error_log(SEV_ERROR, "STORE-ERR-014", "RecoveryStore",
                  "Journal slot unexpectedly occupied - append refused",
                  "journal read-only this boot - no history overwritten");
        return;
    }
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
        dropped++;
        error_log(SEV_WARNING, "STORE-WARN-006", "RecoveryStore",
                  "Journal sector write failed - entry not recorded",
                  "event lost from durable journal");
        return;
    }

    jcount++;
    /* Persist the new count (it lives in the state block). If this save
       is lost, the next boot's scan still finds the entry. */
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
    if (!journal_entry_valid(e))
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

/* ------------------------------------------------------------------ */
/* Integrity status (report / shell)                                    */
/* ------------------------------------------------------------------ */

void recovery_store_integrity(struct recovery_store_integrity *out)
{
    if (!out)
        return;
    out->state_source = state_source;
    out->state_redundancy = mirror_present ? "mirror_lba50" :
                           (ata_available() ? "single_copy_capacity" : "unavailable");
    out->generation = generation;
    out->state_locked = state_locked;
    out->journal_locked = journal_locked;
    out->journal_scan_valid = journal_scan_valid;
    out->journal_full = jcount >= JOURNAL_CAPACITY;
    out->journal_capacity = JOURNAL_CAPACITY;
    out->dropped_appends = dropped;
    out->corrupt_entries = corrupt_slots;
}

uint8_t recovery_journal_last_action_event(uint8_t action_id)
{
    if (!ata_available() || jcount == 0)
        return 0;
    uint8_t buf[ATA_SECTOR_SIZE];
    uint32_t cached = 0xFFFFFFFFu;
    for (uint32_t i = jcount; i-- > 0;) {
        uint32_t lba = LBA_JOURNAL + i / ENTRIES_PER_SECTOR;
        if (lba != cached) {
            if (!ata_read_sector(lba, buf))
                return 0;
            cached = lba;
        }
        const uint8_t *e = buf + (i % ENTRIES_PER_SECTOR) * ENTRY_SIZE;
        if (!journal_entry_valid(e))
            continue;                  /* corrupt slots are skipped, kept */
        if (e[4] == action_id && e[3] >= JE_SUGGESTED && e[3] <= JE_BLOCKED)
            return e[3];
    }
    return 0;
}

uint8_t recovery_store_max_journal_boot(void)
{
    return max_journal_boot;
}

uint8_t recovery_store_boot_lower_bound(void)
{
    return boot_lower_bound;
}
