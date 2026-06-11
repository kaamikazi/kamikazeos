/* package_state.c — persistent package lifecycle records at LBA 48. */
#include "package_state.h"
#include "ata.h"
#include "errors.h"
#include "string.h"

#define LBA_PSTATE 48
#define PS_MAGIC0 'K'
#define PS_MAGIC1 'P'
#define PS_MAGIC2 'S'
#define PS_MAGIC3 'T'
#define PS_VERSION 1
#define HDR 6                              /* magic4 + version + count  */
#define REC (PM_ID_MAX + 8 + PM_VER_MAX*2 + 3) /* id + 8 flag bytes +
                                              versions + result/baselines */

static struct pkg_pstate states[PSTATE_MAX];
static uint32_t count;

void package_state_save(void)
{
    if (!ata_available())
        return;

    uint8_t sec[ATA_SECTOR_SIZE];
    kmemset(sec, 0, sizeof(sec));
    sec[0]=PS_MAGIC0; sec[1]=PS_MAGIC1; sec[2]=PS_MAGIC2; sec[3]=PS_MAGIC3;
    sec[4]=PS_VERSION;
    sec[5]=(uint8_t)count;

    for (uint32_t i = 0; i < count; i++) {
        uint8_t *r = sec + HDR + i * REC;
        const struct pkg_pstate *s = &states[i];
        for (int c = 0; c < PM_ID_MAX; c++) r[c] = (uint8_t)s->package_id[c];
        uint8_t *f = r + PM_ID_MAX;
        f[0]=s->status; f[1]=s->approved; f[2]=s->applied; f[3]=s->verified;
        f[4]=s->failed; f[5]=s->rolled_back;
        f[6]=s->slot_before_apply; f[7]=s->slot_after_apply;
        uint8_t *v = f + 8;
        for (int c = 0; c < PM_VER_MAX; c++) {
            v[c]              = (uint8_t)s->from_version[c];
            v[PM_VER_MAX + c] = (uint8_t)s->to_version[c];
        }
        uint8_t *t = v + PM_VER_MAX * 2;
        t[0]=s->last_result_code; t[1]=s->criticals_at_apply; t[2]=s->health_at_apply;
    }

    int used = HDR + (int)count * REC;
    uint8_t sum = 0;
    for (int i = 0; i < used; i++)
        sum = (uint8_t)(sum + sec[i]);
    sec[used] = (uint8_t)(0u - sum);

    ata_write_sector(LBA_PSTATE, sec);
}

void package_state_init(void)
{
    count = 0;
    kmemset(states, 0, sizeof(states));
    if (!ata_available())
        return;

    uint8_t sec[ATA_SECTOR_SIZE];
    if (!ata_read_sector(LBA_PSTATE, sec))
        return;
    if (sec[0]!=PS_MAGIC0 || sec[1]!=PS_MAGIC1 || sec[2]!=PS_MAGIC2 ||
        sec[3]!=PS_MAGIC3 || sec[4]!=PS_VERSION || sec[5] > PSTATE_MAX)
        return;

    uint32_t n = sec[5];
    int used = HDR + (int)n * REC;
    uint8_t sum = 0;
    for (int i = 0; i <= used; i++)
        sum = (uint8_t)(sum + sec[i]);
    if (sum != 0)
        return;                            /* corrupt: start empty      */

    for (uint32_t i = 0; i < n; i++) {
        const uint8_t *r = sec + HDR + i * REC;
        struct pkg_pstate *s = &states[i];
        for (int c = 0; c < PM_ID_MAX; c++) s->package_id[c] = (char)r[c];
        s->package_id[PM_ID_MAX-1] = '\0';
        const uint8_t *f = r + PM_ID_MAX;
        s->status=f[0]; s->approved=f[1]; s->applied=f[2]; s->verified=f[3];
        s->failed=f[4]; s->rolled_back=f[5];
        s->slot_before_apply=f[6]; s->slot_after_apply=f[7];
        const uint8_t *v = f + 8;
        for (int c = 0; c < PM_VER_MAX; c++) {
            s->from_version[c] = (char)v[c];
            s->to_version[c]   = (char)v[PM_VER_MAX + c];
        }
        s->from_version[PM_VER_MAX-1] = '\0';
        s->to_version[PM_VER_MAX-1]   = '\0';
        const uint8_t *t = v + PM_VER_MAX * 2;
        s->last_result_code=t[0]; s->criticals_at_apply=t[1]; s->health_at_apply=t[2];
    }
    count = n;
    if (count > 0)
        error_log(SEV_INFO, "PST-000", "PackageState",
                  "Persistent package states restored from disk",
                  "package lifecycle survives restarts");
}

void package_state_clear(void)
{
    count = 0;
    kmemset(states, 0, sizeof(states));
    package_state_save();
}

uint32_t package_state_count(void) { return count; }

const struct pkg_pstate *package_state_get(uint32_t index)
{
    return (index < count) ? &states[index] : 0;
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
    return s;
}
