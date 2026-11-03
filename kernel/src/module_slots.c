/* module_slots.c — A/B slot metadata + persistence. See module_slots.h. */
#include "module_slots.h"
#include "ata.h"
#include "errors.h"
#include "string.h"
#include "package_authority.h"

#define LBA_SLOTS 56
#define MS_MAGIC0 'K'
#define MS_MAGIC1 'S'
#define MS_MAGIC2 'L'
#define MS_MAGIC3 'T'
#define MS_VERSION 1
/* per-record size on disk: module(1) + 2 versions + 5 flag bytes */
#define REC_SIZE (1 + SLOT_VER_MAX * 2 + 5)
#define HDR_SIZE 6                       /* magic4 + version + count */

static struct module_slot slots[SLOT_MODULE_COUNT];
static struct module_slot durable[SLOT_MODULE_COUNT];
static enum module_slot_evidence evidence;
static const char *evidence_reason = "not_initialized";

/* Which modules are updatable, and their compiled-in base versions. */
static const struct { enum module_id id; const char *base; } defaults[SLOT_MODULE_COUNT] = {
    { MOD_GHOSTATTACH,     "0.3" },
    { MOD_RECOVERY_ENGINE, "0.5" },
    { MOD_AI_RULES,        "0.4" },
    { MOD_RC_SCHEMA,       "0.8" },
};

int module_slot_factory_record(uint32_t index, struct module_slot *out)
{
    if (index >= SLOT_MODULE_COUNT || !out)
        return 0;
    /* Zero first: every serialized version-array byte after the terminator
       is part of the canonical image. */
    kmemset(out, 0, sizeof(*out));
    out->module = defaults[index].id;
    kstrcpy(out->slot_a_version, defaults[index].base, SLOT_VER_MAX);
    kstrcpy(out->slot_b_version, "-", SLOT_VER_MAX);
    out->active_slot     = SLOT_A;
    out->slot_a_verified = 1;     /* the shipped version is trusted */
    out->slot_b_verified = 0;
    out->rollback_slot   = SLOT_NONE;
    out->pending_verify  = 0;
    return 1;
}

static void load_defaults(void)
{
    for (int i = 0; i < SLOT_MODULE_COUNT; i++)
        module_slot_factory_record((uint32_t)i, &slots[i]);
}

static int slot_text_valid(const char *s)
{
    for (int i = 0; i < SLOT_VER_MAX; i++) {
        if (!s[i]) return i > 0;
        if ((uint8_t)s[i] < 0x20 || (uint8_t)s[i] > 0x7e) return 0;
    }
    return 0;
}

int module_slot_record_valid(const struct module_slot *s)
{
    if (!s || !slot_text_valid(s->slot_a_version) || !slot_text_valid(s->slot_b_version) ||
        s->active_slot > SLOT_B || s->slot_a_verified > 1 || s->slot_b_verified > 1 ||
        s->pending_verify > 1 ||
        (s->rollback_slot != SLOT_NONE && s->rollback_slot > SLOT_B) ||
        s->rollback_slot == s->active_slot)
        return 0;
    int active_verified = s->active_slot == SLOT_A ? s->slot_a_verified : s->slot_b_verified;
    if (kstrcmp(module_slot_active_version(s), "-") == 0 ||
        active_verified == s->pending_verify)
        return 0;
    if (s->pending_verify && s->rollback_slot == SLOT_NONE)
        return 0;
    if (s->rollback_slot != SLOT_NONE) {
        const char *v = s->rollback_slot == SLOT_A ? s->slot_a_version : s->slot_b_version;
        int verified = s->rollback_slot == SLOT_A ? s->slot_a_verified : s->slot_b_verified;
        if (!verified || kstrcmp(v, "-") == 0) return 0;
    }
    return 1;
}

int module_slot_is_pristine(const struct module_slot *s)
{
    for (int i = 0; i < SLOT_MODULE_COUNT; i++)
        if (s->module == defaults[i].id)
            return s->active_slot == SLOT_A && s->slot_a_verified == 1 &&
                s->slot_b_verified == 0 && s->rollback_slot == SLOT_NONE && !s->pending_verify &&
                kstrcmp(s->slot_a_version, defaults[i].base) == 0 && kstrcmp(s->slot_b_version, "-") == 0;
    return 0;
}

static void slot_uncertain(const char *reason)
{
    evidence = MS_UNCERTAIN;
    evidence_reason = reason;
    package_authority_note_uncertain(reason);
    error_log(SEV_ERROR, "SLOT-ERR-001", "ModuleSlots",
              "Module-slot evidence uncertain - writes held", reason);
}

/* Push the active slot versions into the module version table, so
   `versions`, reports, and from_version checks all see slot truth. */
static void sync_module_versions(void)
{
    for (int i = 0; i < SLOT_MODULE_COUNT; i++)
        module_set_version(slots[i].module,
                           module_slot_active_version(&slots[i]));
}

int module_slots_save(void)
{
    /* Authority first: a refused attempt must consume any reset grant. */
    if (!package_authority_allow_slot_write())
        return 0;
    if (!ata_available())
        return 0;

    uint8_t sec[ATA_SECTOR_SIZE];
    kmemset(sec, 0, sizeof(sec));
    sec[0] = MS_MAGIC0; sec[1] = MS_MAGIC1; sec[2] = MS_MAGIC2; sec[3] = MS_MAGIC3;
    sec[4] = MS_VERSION;
    sec[5] = SLOT_MODULE_COUNT;

    for (int i = 0; i < SLOT_MODULE_COUNT; i++) {
        uint8_t *r = sec + HDR_SIZE + i * REC_SIZE;
        r[0] = (uint8_t)slots[i].module;
        for (int c = 0; c < SLOT_VER_MAX; c++) {
            r[1 + c]                = (uint8_t)slots[i].slot_a_version[c];
            r[1 + SLOT_VER_MAX + c] = (uint8_t)slots[i].slot_b_version[c];
        }
        uint8_t *f = r + 1 + SLOT_VER_MAX * 2;
        f[0] = slots[i].active_slot;
        f[1] = slots[i].slot_a_verified;
        f[2] = slots[i].slot_b_verified;
        f[3] = slots[i].rollback_slot;
        f[4] = slots[i].pending_verify;
    }

    /* trailing checksum over the used area */
    int used = HDR_SIZE + SLOT_MODULE_COUNT * REC_SIZE;
    uint8_t sum = 0;
    for (int i = 0; i < used; i++)
        sum = (uint8_t)(sum + sec[i]);
    sec[used] = (uint8_t)(0u - sum);

    if (!ata_write_sector(LBA_SLOTS, sec)) {
        slot_uncertain("slot_write_failure");
        return 0;
    }
    memcpy(durable, slots, sizeof(durable));
    evidence = MS_VALID;
    evidence_reason = "slot_write_acknowledged";
    sync_module_versions();
    return 1;
}

void module_slots_init(void)
{
    load_defaults();
    evidence = MS_UNAVAILABLE;
    evidence_reason = "slot_unavailable";
    if (!ata_available() || ata_capacity_sectors() <= LBA_SLOTS) return;
    uint8_t sec[ATA_SECTOR_SIZE];
    if (!ata_read_sector(LBA_SLOTS, sec)) { slot_uncertain("slot_read_failure"); return; }
    int occupied = 0;
    for (uint32_t i = 0; i < sizeof(sec); i++) occupied |= sec[i];
    if (!occupied) {
        evidence = MS_EMPTY;
        evidence_reason = "slot_sector_read_empty";
        memcpy(durable, slots, sizeof(durable));
        return; /* defaults are not current versions or mutation authority */
    }
    int used = HDR_SIZE + SLOT_MODULE_COUNT * REC_SIZE;
    uint8_t sum = 0;
    for (int i = 0; i <= used; i++) sum += sec[i];
    if (sec[0] != MS_MAGIC0 || sec[1] != MS_MAGIC1 || sec[2] != MS_MAGIC2 ||
        sec[3] != MS_MAGIC3 || sec[4] != MS_VERSION || sec[5] != SLOT_MODULE_COUNT || sum) {
        slot_uncertain("slot_format_or_checksum"); return;
    }
    struct module_slot parsed[SLOT_MODULE_COUNT];
    kmemset(parsed, 0, sizeof(parsed));
    for (int i = 0; i < SLOT_MODULE_COUNT; i++) {
        const uint8_t *r = sec + HDR_SIZE + i * REC_SIZE;
        parsed[i].module = (enum module_id)r[0];
        memcpy(parsed[i].slot_a_version, r + 1, SLOT_VER_MAX);
        memcpy(parsed[i].slot_b_version, r + 1 + SLOT_VER_MAX, SLOT_VER_MAX);
        const uint8_t *f = r + 1 + SLOT_VER_MAX * 2;
        parsed[i].active_slot = f[0]; parsed[i].slot_a_verified = f[1]; parsed[i].slot_b_verified = f[2];
        parsed[i].rollback_slot = f[3]; parsed[i].pending_verify = f[4];
        if (r[0] != defaults[i].id || !module_slot_record_valid(&parsed[i])) {
            slot_uncertain("slot_record_inconsistent"); return;
        }
    }
    memcpy(slots, parsed, sizeof(slots));
    memcpy(durable, slots, sizeof(durable));
    evidence = MS_VALID;
    evidence_reason = "slot_sector_validated";
    sync_module_versions();
    error_log(SEV_INFO, "SLOT-000", "ModuleSlots", "Module slot metadata restored from disk",
              "joint ownership must also validate package records");
}

int module_slots_clear(void)
{
    if (!package_authority_reset_active()) return 0;
    struct module_slot previous[SLOT_MODULE_COUNT];
    memcpy(previous, slots, sizeof(previous));
    load_defaults();
    if (!module_slots_save()) {
        memcpy(slots, previous, sizeof(previous));
        sync_module_versions();
        return 0;
    }
    return 1;
}

uint32_t module_slot_count(void) { return SLOT_MODULE_COUNT; }

const struct module_slot *module_slot_get(uint32_t index)
{
    return (index < SLOT_MODULE_COUNT) ? &slots[index] : 0;
}

struct module_slot *module_slot_for(enum module_id id)
{
    if (evidence == MS_UNAVAILABLE || evidence == MS_UNCERTAIN) return 0;
    for (int i = 0; i < SLOT_MODULE_COUNT; i++)
        if (slots[i].module == id)
            return &slots[i];
    return 0;
}

enum module_slot_evidence module_slots_evidence(void) { return evidence; }
const char *module_slots_evidence_reason(void) { return evidence_reason; }
const struct module_slot *module_slot_durable_get(uint32_t index)
{
    if (index >= SLOT_MODULE_COUNT || evidence == MS_UNAVAILABLE || evidence == MS_UNCERTAIN) return 0;
    return &durable[index];
}

const char *module_slot_active_version(const struct module_slot *s)
{
    return (s->active_slot == SLOT_B) ? s->slot_b_version
                                      : s->slot_a_version;
}

int module_slots_pending_verify(void)
{
    for (int i = 0; i < SLOT_MODULE_COUNT; i++)
        if (slots[i].pending_verify)
            return 1;
    return 0;
}
