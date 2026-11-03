/* package_state.c - mirrored persistent lifecycle state at LBA 48-49. */
#include "package_state.h"
#include "ata.h"
#include "errors.h"
#include "string.h"

#define LBA_PSTATE_PRIMARY 48
#define LBA_PSTATE_MIRROR  49
#define PS_MAGIC0 'K'
#define PS_MAGIC1 'P'
#define PS_MAGIC2 'S'
#define PS_MAGIC3 'T'
#define PS_VERSION 1
#define HDR 6
#define REC (PM_ID_MAX + 8 + PM_VER_MAX * 2 + 3)

/* The v1 record and checksum end by byte 302. The transaction extension uses
   reserved bytes, so old kernels still read the primary sector unchanged. */
#define TX_OFF 320
#define TX_HDR 10
#define TX_REC 8
#define TX_VERSION 1

struct decoded_state {
    struct pkg_pstate states[PSTATE_MAX];
    uint32_t count;
    uint32_t generation;
    int valid;
    int extension_valid;
};

static struct pkg_pstate states[PSTATE_MAX];
static uint32_t count;
static uint32_t generation;

const char *package_transaction_phase_name(uint8_t phase)
{
    switch (phase) {
    case PST_TX_LEGACY:          return "legacy_unbound";
    case PST_TX_COMMITTED:       return "committed";
    case PST_TX_APPLY_STAGED:    return "apply_staged";
    case PST_TX_VERIFY_STAGED:   return "verify_staged";
    case PST_TX_ROLLBACK_STAGED: return "rollback_staged";
    }
    return "invalid";
}

static void put_u32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static uint32_t get_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint8_t checksum_over(const uint8_t *data, uint32_t length)
{
    uint8_t sum = 0;
    for (uint32_t i = 0; i < length; i++)
        sum = (uint8_t)(sum + data[i]);
    return (uint8_t)(0u - sum);
}

static void encode_sector(uint8_t *sec)
{
    kmemset(sec, 0, ATA_SECTOR_SIZE);
    sec[0] = PS_MAGIC0; sec[1] = PS_MAGIC1;
    sec[2] = PS_MAGIC2; sec[3] = PS_MAGIC3;
    sec[4] = PS_VERSION;
    sec[5] = (uint8_t)count;

    for (uint32_t i = 0; i < count; i++) {
        uint8_t *r = sec + HDR + i * REC;
        const struct pkg_pstate *s = &states[i];
        for (int c = 0; c < PM_ID_MAX; c++) r[c] = (uint8_t)s->package_id[c];
        uint8_t *f = r + PM_ID_MAX;
        f[0] = s->status; f[1] = s->approved; f[2] = s->applied;
        f[3] = s->verified; f[4] = s->failed; f[5] = s->rolled_back;
        f[6] = s->slot_before_apply; f[7] = s->slot_after_apply;
        uint8_t *v = f + 8;
        for (int c = 0; c < PM_VER_MAX; c++) {
            v[c] = (uint8_t)s->from_version[c];
            v[PM_VER_MAX + c] = (uint8_t)s->to_version[c];
        }
        uint8_t *t = v + PM_VER_MAX * 2;
        t[0] = s->last_result_code;
        t[1] = s->criticals_at_apply;
        t[2] = s->health_at_apply;
    }

    uint32_t used = HDR + count * REC;
    sec[used] = checksum_over(sec, used);

    uint8_t *tx = sec + TX_OFF;
    tx[0] = 'K'; tx[1] = 'T'; tx[2] = 'X'; tx[3] = 'N';
    tx[4] = TX_VERSION;
    tx[5] = (uint8_t)count;
    put_u32(tx + 6, generation);
    for (uint32_t i = 0; i < count; i++) {
        uint8_t *r = tx + TX_HDR + i * TX_REC;
        put_u32(r, states[i].manifest_identity);
        r[4] = states[i].target_module;
        r[5] = states[i].transaction_phase;
        r[6] = states[i].status_before_transaction;
        r[7] = states[i].extension_valid ? 1 : 0;
    }
    uint32_t tx_used = TX_HDR + count * TX_REC;
    tx[tx_used] = checksum_over(tx, tx_used);
}

static void decode_sector(const uint8_t *sec, struct decoded_state *out)
{
    kmemset(out, 0, sizeof(*out));
    if (sec[0] != PS_MAGIC0 || sec[1] != PS_MAGIC1 ||
        sec[2] != PS_MAGIC2 || sec[3] != PS_MAGIC3 ||
        sec[4] != PS_VERSION || sec[5] > PSTATE_MAX)
        return;

    uint32_t n = sec[5];
    uint32_t used = HDR + n * REC;
    uint8_t sum = 0;
    for (uint32_t i = 0; i <= used; i++)
        sum = (uint8_t)(sum + sec[i]);
    if (sum != 0)
        return;

    for (uint32_t i = 0; i < n; i++) {
        const uint8_t *r = sec + HDR + i * REC;
        struct pkg_pstate *s = &out->states[i];
        for (int c = 0; c < PM_ID_MAX; c++) s->package_id[c] = (char)r[c];
        s->package_id[PM_ID_MAX - 1] = '\0';
        const uint8_t *f = r + PM_ID_MAX;
        if (f[0] > 7 || f[1] > 1 || f[2] > 1 || f[3] > 1 ||
            f[4] > 1 || f[5] > 1)
            return;
        s->status = f[0]; s->approved = f[1]; s->applied = f[2];
        s->verified = f[3]; s->failed = f[4]; s->rolled_back = f[5];
        s->slot_before_apply = f[6]; s->slot_after_apply = f[7];
        const uint8_t *v = f + 8;
        for (int c = 0; c < PM_VER_MAX; c++) {
            s->from_version[c] = (char)v[c];
            s->to_version[c] = (char)v[PM_VER_MAX + c];
        }
        s->from_version[PM_VER_MAX - 1] = '\0';
        s->to_version[PM_VER_MAX - 1] = '\0';
        const uint8_t *t = v + PM_VER_MAX * 2;
        s->last_result_code = t[0];
        s->criticals_at_apply = t[1];
        s->health_at_apply = t[2];
    }
    out->count = n;
    out->valid = 1;

    const uint8_t *tx = sec + TX_OFF;
    if (tx[0] != 'K' || tx[1] != 'T' || tx[2] != 'X' || tx[3] != 'N' ||
        tx[4] != TX_VERSION || tx[5] != n)
        return;
    uint32_t tx_used = TX_HDR + n * TX_REC;
    uint8_t tx_sum = 0;
    for (uint32_t i = 0; i <= tx_used; i++)
        tx_sum = (uint8_t)(tx_sum + tx[i]);
    if (tx_sum != 0)
        return;

    out->generation = get_u32(tx + 6);
    out->extension_valid = 1;
    for (uint32_t i = 0; i < n; i++) {
        const uint8_t *r = tx + TX_HDR + i * TX_REC;
        if (r[7] != 1 || r[5] > PST_TX_ROLLBACK_STAGED ||
            r[4] >= MODULE_COUNT || r[6] > 7) {
            out->states[i].extension_valid = 0;
            continue;
        }
        out->states[i].manifest_identity = get_u32(r);
        out->states[i].target_module = r[4];
        out->states[i].transaction_phase = r[5];
        out->states[i].status_before_transaction = r[6];
        out->states[i].extension_valid = 1;
    }
}

static int generation_newer(uint32_t a, uint32_t b)
{
    return (int32_t)(a - b) > 0;
}

int package_state_save(void)
{
    if (!ata_available())
        return 0;

    generation++;
    uint8_t sec[ATA_SECTOR_SIZE];
    encode_sector(sec);
    int mirror_ok = ata_write_sector(LBA_PSTATE_MIRROR, sec);
    int primary_ok = ata_write_sector(LBA_PSTATE_PRIMARY, sec);
    if (!mirror_ok && !primary_ok) {
        generation--;
        error_log(SEV_ERROR, "PST-ERR-001", "PackageState",
                  "Both package state copies failed to write",
                  "lifecycle transition was not committed");
        return 0;
    }
    if (!mirror_ok || !primary_ok)
        error_log(SEV_WARNING, "PST-WARN-002", "PackageState",
                  "Only one package state copy was persisted",
                  "state is durable but redundancy is degraded");
    return 1;
}

void package_state_init(void)
{
    count = 0;
    generation = 0;
    kmemset(states, 0, sizeof(states));
    if (!ata_available())
        return;

    uint8_t primary_sec[ATA_SECTOR_SIZE];
    uint8_t mirror_sec[ATA_SECTOR_SIZE];
    struct decoded_state primary;
    struct decoded_state mirror;
    if (!ata_read_sector(LBA_PSTATE_PRIMARY, primary_sec))
        kmemset(primary_sec, 0, sizeof(primary_sec));
    if (!ata_read_sector(LBA_PSTATE_MIRROR, mirror_sec))
        kmemset(mirror_sec, 0, sizeof(mirror_sec));
    decode_sector(primary_sec, &primary);
    decode_sector(mirror_sec, &mirror);

    const struct decoded_state *selected = 0;
    if (primary.valid && mirror.valid) {
        if (mirror.extension_valid &&
            (!primary.extension_valid || generation_newer(mirror.generation,
                                                          primary.generation)))
            selected = &mirror;
        else
            selected = &primary;
    } else if (primary.valid) {
        selected = &primary;
    } else if (mirror.valid) {
        selected = &mirror;
    }
    if (!selected)
        return;

    memcpy(states, selected->states, sizeof(states));
    count = selected->count;
    generation = selected->generation;
    error_log(SEV_INFO, "PST-000", "PackageState",
              "Persistent package states restored from disk",
              selected->extension_valid
                  ? "newest valid mirrored transaction state selected"
                  : "legacy state loaded approvals require revalidation");
}

int package_state_clear(void)
{
    count = 0;
    kmemset(states, 0, sizeof(states));
    return package_state_save();
}

uint32_t package_state_count(void) { return count; }

const struct pkg_pstate *package_state_get(uint32_t index)
{
    return index < count ? &states[index] : 0;
}

struct pkg_pstate *package_state_find(const char *package_id)
{
    for (uint32_t i = 0; i < count; i++)
        if (kstricmp(states[i].package_id, package_id) == 0)
            return &states[i];
    return 0;
}

struct pkg_pstate *package_state_ensure(const char *package_id)
{
    struct pkg_pstate *s = package_state_find(package_id);
    if (s)
        return s;
    if (count >= PSTATE_MAX)
        return 0;
    s = &states[count++];
    kmemset(s, 0, sizeof(*s));
    kstrcpy(s->package_id, package_id, PM_ID_MAX);
    s->transaction_phase = PST_TX_LEGACY;
    return s;
}
