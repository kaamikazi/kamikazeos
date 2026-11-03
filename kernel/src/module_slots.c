/* module_slots.c — A/B slot metadata + persistence. See module_slots.h. */
#include "module_slots.h"
#include "ata.h"
#include "errors.h"
#include "string.h"

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

/* Which modules are updatable, and their compiled-in base versions. */
static const struct { enum module_id id; const char *base; } defaults[SLOT_MODULE_COUNT] = {
    { MOD_GHOSTATTACH,     "0.3" },
    { MOD_RECOVERY_ENGINE, "0.5" },
    { MOD_AI_RULES,        "0.4" },
    { MOD_RC_SCHEMA,       "0.8" },
};

static void load_defaults(void)
{
    for (int i = 0; i < SLOT_MODULE_COUNT; i++) {
        kmemset(&slots[i], 0, sizeof(slots[i]));
        slots[i].module = defaults[i].id;
        kstrcpy(slots[i].slot_a_version, defaults[i].base, SLOT_VER_MAX);
        kstrcpy(slots[i].slot_b_version, "-", SLOT_VER_MAX);
        slots[i].active_slot     = SLOT_A;
        slots[i].slot_a_verified = 1;     /* the shipped version is trusted */
        slots[i].slot_b_verified = 0;
        slots[i].rollback_slot   = SLOT_NONE;
        slots[i].pending_verify  = 0;
    }
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

    return ata_write_sector(LBA_SLOTS, sec);
}

void module_slots_init(void)
{
    load_defaults();

    if (ata_available()) {
        uint8_t sec[ATA_SECTOR_SIZE];
        if (ata_read_sector(LBA_SLOTS, sec) &&
            sec[0] == MS_MAGIC0 && sec[1] == MS_MAGIC1 &&
            sec[2] == MS_MAGIC2 && sec[3] == MS_MAGIC3 &&
            sec[4] == MS_VERSION && sec[5] == SLOT_MODULE_COUNT) {

            int used = HDR_SIZE + SLOT_MODULE_COUNT * REC_SIZE;
            uint8_t sum = 0;
            for (int i = 0; i <= used; i++)
                sum = (uint8_t)(sum + sec[i]);

            if (sum == 0) {
                for (int i = 0; i < SLOT_MODULE_COUNT; i++) {
                    const uint8_t *r = sec + HDR_SIZE + i * REC_SIZE;
                    /* module id must match our table position — a layout
                       change in a future kernel falls back to defaults. */
                    if (r[0] != (uint8_t)defaults[i].id) {
                        load_defaults();
                        goto synced;
                    }
                    for (int c = 0; c < SLOT_VER_MAX; c++) {
                        slots[i].slot_a_version[c] = (char)r[1 + c];
                        slots[i].slot_b_version[c] = (char)r[1 + SLOT_VER_MAX + c];
                    }
                    slots[i].slot_a_version[SLOT_VER_MAX - 1] = '\0';
                    slots[i].slot_b_version[SLOT_VER_MAX - 1] = '\0';
                    const uint8_t *f = r + 1 + SLOT_VER_MAX * 2;
                    slots[i].active_slot     = f[0] == SLOT_B ? SLOT_B : SLOT_A;
                    slots[i].slot_a_verified = f[1] ? 1 : 0;
                    slots[i].slot_b_verified = f[2] ? 1 : 0;
                    if (f[3] != SLOT_NONE && f[3] != SLOT_A && f[3] != SLOT_B) {
                        load_defaults();
                        goto synced;
                    }
                    slots[i].rollback_slot   = f[3];
                    slots[i].pending_verify  = f[4] ? 1 : 0;
                }
                error_log(SEV_INFO, "SLOT-000", "ModuleSlots",
                          "Module slot metadata restored from disk",
                          "active slots define the running versions");
            }
        }
    }

synced:
    sync_module_versions();
}

int module_slots_clear(void)
{
    struct module_slot previous[SLOT_MODULE_COUNT];
    memcpy(previous, slots, sizeof(previous));
    load_defaults();
    sync_module_versions();
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
    for (int i = 0; i < SLOT_MODULE_COUNT; i++)
        if (slots[i].module == id)
            return &slots[i];
    return 0;
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
