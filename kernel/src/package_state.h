/*
 * package_state.h — persistent per-package lifecycle state.
 *
 * v0.9's package statuses evaporated at reboot; only the journal
 * remembered. v1.0 persists the lifecycle itself (LBA 48, checksummed):
 * approved/applied/verified/failed/rolled_back survive a full restart,
 * along with the slot transition and health baseline needed to verify
 * or roll back an update applied BEFORE the reboot. That's the property
 * a real updater needs most: the machine can die mid-update and still
 * know exactly where it stood.
 */
#ifndef KMZ_PACKAGE_STATE_H
#define KMZ_PACKAGE_STATE_H

#include <stdint.h>
#include "package_manifest.h"     /* PM_ID_MAX, PM_VER_MAX */

#define PSTATE_MAX 8

struct pkg_pstate {
    char    package_id[PM_ID_MAX];
    uint8_t status;               /* enum pkg_status, persisted          */
    uint8_t approved;
    uint8_t applied;
    uint8_t verified;
    uint8_t failed;
    uint8_t rolled_back;
    uint8_t slot_before_apply;    /* SLOT_A/SLOT_B/SLOT_NONE             */
    uint8_t slot_after_apply;
    char    from_version[PM_VER_MAX];
    char    to_version[PM_VER_MAX];
    uint8_t last_result_code;     /* last lifecycle verb's return        */
    uint8_t criticals_at_apply;   /* verification baselines              */
    uint8_t health_at_apply;
};

void package_state_init(void);             /* load LBA 48 (or empty)    */
void package_state_save(void);
void package_state_clear(void);            /* wipe + save               */

uint32_t                 package_state_count(void);
const struct pkg_pstate *package_state_get(uint32_t index);
struct pkg_pstate       *package_state_find(const char *package_id);
/* find-or-create (returns 0 only if the table is full) */
struct pkg_pstate       *package_state_ensure(const char *package_id);

#endif /* KMZ_PACKAGE_STATE_H */
