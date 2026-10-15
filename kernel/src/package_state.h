/*
 * package_state.h — persistent per-package lifecycle state.
 *
 * v0.9's package statuses evaporated at reboot; only the journal
 * remembered. v1.0 persists the lifecycle itself (LBA 48, checksummed):
 * approved/applied/verified/failed/rolled_back survive a full restart,
 * along with the slot transition and health baseline needed to verify
 * or roll back an update applied BEFORE the reboot. That's the property
 * Hardened builds retain that base format, add transaction metadata in its
 * reserved tail, and mirror the sector at LBA 49. Logical transitions are
 * staged before slot metadata changes and reconciled after interruption.
 * This remains metadata-only and is not a general transactional filesystem.
 */
#ifndef KMZ_PACKAGE_STATE_H
#define KMZ_PACKAGE_STATE_H

#include <stdint.h>
#include "package_manifest.h"     /* PM_ID_MAX, PM_VER_MAX */

#define PSTATE_MAX 8

enum package_tx_phase {
    PST_TX_LEGACY = 0,
    PST_TX_COMMITTED,
    PST_TX_APPLY_STAGED,
    PST_TX_VERIFY_STAGED,
    PST_TX_ROLLBACK_STAGED,
};

struct pkg_pstate {
    char    package_id[PM_ID_MAX];
    uint8_t status;               /* enum pkg_status, persisted          */
    uint8_t approved;
    uint8_t applied;
    uint8_t verified;
    uint8_t failed;
    uint8_t rolled_back;
    uint8_t slot_before_apply;    /* A/B rollback right; NONE if superseded */
    uint8_t slot_after_apply;
    char    from_version[PM_VER_MAX];
    char    to_version[PM_VER_MAX];
    uint8_t last_result_code;     /* last lifecycle verb's return        */
    uint8_t criticals_at_apply;   /* verification baselines              */
    uint8_t health_at_apply;
    uint32_t manifest_identity;
    uint8_t target_module;
    uint8_t transaction_phase;
    uint8_t status_before_transaction;
    uint8_t extension_valid;
};

void package_state_init(void);             /* load LBA 48 (or empty)    */
int package_state_save(void);
int package_state_clear(void);             /* wipe + save               */

/* Runtime JOINT trust, not package-copy quorum or new on-disk fields.
   Slot uncertainty/contradiction also denies every ownership mutation. */
int package_state_ownership_trusted(void);
const char *package_state_ownership_state(void);
const char *package_state_ownership_reason(void);

/* Copy integrity is evidence, not ownership authority. Durable views are the
   last acknowledged/loaded values, independent of mutable proposals. */
int package_state_copies_trusted(void);
const char *package_state_copy_state(void);
const char *package_state_copy_reason(void);
uint32_t package_state_durable_count(void);
const struct pkg_pstate *package_state_durable_get(uint32_t index);

uint32_t                 package_state_count(void);
const struct pkg_pstate *package_state_get(uint32_t index);
struct pkg_pstate       *package_state_find(const char *package_id);
/* find-or-create (returns 0 only if the table is full) */
struct pkg_pstate       *package_state_ensure(const char *package_id);
const char *package_transaction_phase_name(uint8_t phase);

#endif /* KMZ_PACKAGE_STATE_H */
