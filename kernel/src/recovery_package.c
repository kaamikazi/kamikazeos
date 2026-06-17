/* recovery_package.c — the v1.0 update pipeline engine.
 * Read recovery_package.h for the pipeline and the honesty labels. */
#include "recovery_package.h"
#include "boot_snapshot.h"
#include "errors.h"
#include "module_slots.h"
#include "recovery_store.h"
#include "string.h"

/* Runtime status per manifest slot (hydrated from persistence at init). */
static enum pkg_status statuses[MANIFEST_MAX];

const char *pkg_status_name(enum pkg_status s)
{
    switch (s) {
    case PKGST_AVAILABLE:   return "available";
    case PKGST_SUGGESTED:   return "suggested";
    case PKGST_APPROVED:    return "approved";
    case PKGST_APPLIED:     return "applied";
    case PKGST_VERIFIED:    return "verified";
    case PKGST_FAILED:      return "failed";
    case PKGST_ROLLED_BACK: return "rolled_back";
    case PKGST_BLOCKED:     return "blocked";
    }
    return "?";
}

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */

static int manifest_index(const struct pkg_manifest *m)
{
    for (uint32_t i = 0; i < manifest_count(); i++)
        if (manifest_get(i) == m)
            return (int)i;
    return -1;
}

enum pkg_status recovery_package_status(const struct pkg_manifest *m)
{
    int i = manifest_index(m);
    return (i >= 0) ? statuses[i] : PKGST_AVAILABLE;
}

static void set_status(const struct pkg_manifest *m, enum pkg_status st)
{
    int i = manifest_index(m);
    if (i >= 0)
        statuses[i] = st;
}

static uint8_t pkg_byte(const struct pkg_manifest *m)
{
    uint32_t n = 0;
    const char *p = m->package_id;
    while (*p && (*p < '0' || *p > '9'))
        p++;
    while (*p >= '0' && *p <= '9') {
        n = n * 10u + (uint32_t)(*p - '0');
        p++;
    }
    return (n > 255u) ? 255u : (uint8_t)n;
}

static void log_pkg(enum err_severity sev, const char *code,
                    const char *verb, const struct pkg_manifest *m,
                    const char *hint)
{
    char msg[ERR_MSG_MAX];
    kstrcpy(msg, verb, ERR_MSG_MAX);
    size_t len = kstrlen(msg);
    if (len < ERR_MSG_MAX - 1) { msg[len++] = ' '; msg[len] = '\0';
        kstrcpy(msg + len, m->package_id, ERR_MSG_MAX - len); }
    len = kstrlen(msg);
    if (len < ERR_MSG_MAX - 1) { msg[len++] = ' '; msg[len] = '\0';
        kstrcpy(msg + len, m->name, ERR_MSG_MAX - len); }
    error_log(sev, code, "RecoveryPackage", msg, hint);
}

static void journal_pkg(enum journal_event ev, const struct pkg_manifest *m,
                        uint8_t result, const char *code6)
{
    recovery_journal_append(ev, pkg_byte(m), result,
                            (uint8_t)m->risk_level, m->reversible, code6);
}

/* Persist one lifecycle transition. */
static void persist(const struct pkg_manifest *m, enum pkg_status st,
                    int result_code)
{
    struct pkg_pstate *s = package_state_ensure(m->package_id);
    if (!s)
        return;
    s->status           = (uint8_t)st;
    s->last_result_code = (uint8_t)(result_code & 0xFF);
    kstrcpy(s->from_version, m->from_version, PM_VER_MAX);
    kstrcpy(s->to_version, m->to_version, PM_VER_MAX);
    switch (st) {
    case PKGST_APPROVED:    s->approved = 1;                  break;
    case PKGST_APPLIED:     s->applied = 1;                   break;
    case PKGST_VERIFIED:    s->verified = 1;                  break;
    case PKGST_FAILED:      s->failed = 1;                    break;
    case PKGST_ROLLED_BACK: s->rolled_back = 1;               break;
    default:                                                  break;
    }
    package_state_save();
}

/* ------------------------------------------------------------------ */
/* Init                                                                 */
/* ------------------------------------------------------------------ */

void recovery_package_init(void)
{
    module_versions_init();       /* reset to compiled-in versions    */
    package_manifest_init();
    module_slots_init();          /* overrides with active-slot truth */
    package_state_init();

    for (uint32_t i = 0; i < manifest_count(); i++) {
        struct pkg_manifest *m = manifest_get(i);
        statuses[i] = PKGST_AVAILABLE;

        /* Verification chain first: failures are visibly blocked. */
        const char *reason;
        if (!manifest_verify(m, &reason)) {
            statuses[i] = PKGST_BLOCKED;
            log_pkg(SEV_WARNING, "PKG-BLOCK-001", "Blocked package", m, reason);
            continue;
        }

        /* Hydrate persisted lifecycle state (survives reboots). */
        const struct pkg_pstate *s = package_state_find(m->package_id);
        if (s && s->status <= PKGST_BLOCKED) {
            statuses[i] = (enum pkg_status)s->status;
            continue;
        }

        /* Fresh package: suggest it if it upgrades the running version. */
        if (kstrcmp(module_version_of(m->target_module),
                    m->from_version) == 0) {
            statuses[i] = PKGST_SUGGESTED;
            log_pkg(SEV_INFO, "PKG-SUG-001", "Update available", m,
                    "approve-pkg then apply-pkg then verify-pkg");
        }
    }

    /* An update applied before a reboot but never verified is the single
       most important thing to surface at boot. */
    if (module_slots_pending_verify()) {
        error_log(SEV_WARNING, "PKG-PENDING-VERIFY", "RecoveryPackage",
                  "An applied update is awaiting verification",
                  "run verify-pkg (or rollback-pkg) for the applied package");
    }
}

/* ------------------------------------------------------------------ */
/* Lifecycle verbs                                                      */
/* ------------------------------------------------------------------ */

int pkg_approve(const char *id)
{
    struct pkg_manifest *m = manifest_find(id);
    if (!m)
        return 0;

    const char *reason;
    if (recovery_package_status(m) == PKGST_BLOCKED ||
        !manifest_verify(m, &reason)) {
        manifest_verify(m, &reason);
        log_pkg(SEV_WARNING, "PKG-BLOCK-001", "Blocked package", m, reason);
        journal_pkg(JE_PKG_BLOCKED, m, 0, "PKGBLK");
        return -1;
    }

    set_status(m, PKGST_APPROVED);
    persist(m, PKGST_APPROVED, 1);
    log_pkg(SEV_INFO, "PKG-ACT-001", "Approved package", m,
            "apply-pkg to switch the module slot");
    journal_pkg(JE_PKG_APPROVED, m, 1, "PKGAPR");
    return 1;
}

int pkg_apply(const char *id)
{
    struct pkg_manifest *m = manifest_find(id);
    if (!m)
        return 0;

    const char *reason;
    if (recovery_package_status(m) == PKGST_BLOCKED ||
        !manifest_verify(m, &reason)) {           /* gate, again */
        journal_pkg(JE_PKG_BLOCKED, m, 0, "PKGBLK");
        return -1;
    }
    if (recovery_package_status(m) != PKGST_APPROVED)
        return -2;

    struct module_slot *slot = module_slot_for(m->target_module);
    if (!slot)
        return -1;
    /* The active slot must run the version this package upgrades FROM. */
    if (kstrcmp(module_slot_active_version(slot), m->from_version) != 0)
        return -3;

    /* THE A/B SWITCH: stage the new version in the INACTIVE slot, flip
       the active pointer, keep the old slot untouched as the way back. */
    uint8_t old_slot = slot->active_slot;
    uint8_t new_slot = (old_slot == SLOT_A) ? SLOT_B : SLOT_A;
    if (new_slot == SLOT_A) {
        kstrcpy(slot->slot_a_version, m->to_version, SLOT_VER_MAX);
        slot->slot_a_verified = 0;
    } else {
        kstrcpy(slot->slot_b_version, m->to_version, SLOT_VER_MAX);
        slot->slot_b_verified = 0;
    }
    slot->active_slot    = new_slot;
    slot->rollback_slot  = old_slot;
    slot->pending_verify = 1;
    module_set_version(m->target_module, m->to_version);
    module_slots_save();

    /* Baselines for verify-pkg, persisted with the state. */
    struct health_breakdown hb;
    boot_snapshot_health_now(&hb);
    set_status(m, PKGST_APPLIED);
    persist(m, PKGST_APPLIED, 1);
    struct pkg_pstate *s = package_state_find(m->package_id);
    if (s) {
        s->slot_before_apply  = old_slot;
        s->slot_after_apply   = new_slot;
        s->criticals_at_apply = (uint8_t)(error_count_severity(SEV_ERROR) +
                                          error_count_severity(SEV_PANIC));
        s->health_at_apply    = (uint8_t)hb.score;
        package_state_save();
    }

    log_pkg(SEV_INFO, "PKG-ACT-002", "Applied package - slot switched", m,
            "pending verification - run verify-pkg");
    journal_pkg(JE_PKG_APPLIED, m, 1, "PKGAPL");
    return 1;
}

int pkg_verify(const char *id)
{
    struct pkg_manifest *m = manifest_find(id);
    if (!m)
        return 0;
    if (recovery_package_status(m) != PKGST_APPLIED)
        return -1;

    struct module_slot *slot = module_slot_for(m->target_module);
    struct pkg_pstate  *s    = package_state_find(m->package_id);
    if (!slot || !s)
        return -1;

    /* Checks: slot as expected, version as expected, no new criticals,
       and health didn't fall off a cliff (threshold: -20). */
    struct health_breakdown hb;
    boot_snapshot_health_now(&hb);
    uint32_t criticals_now = error_count_severity(SEV_ERROR) +
                             error_count_severity(SEV_PANIC);

    int slot_ok    = (slot->active_slot == s->slot_after_apply);
    int version_ok = (kstrcmp(module_slot_active_version(slot),
                              m->to_version) == 0);
    int crit_ok    = (criticals_now <= s->criticals_at_apply);
    int health_ok  = (hb.score >= (int)s->health_at_apply - 20);

    if (slot_ok && version_ok && crit_ok && health_ok) {
        if (slot->active_slot == SLOT_A)
            slot->slot_a_verified = 1;
        else
            slot->slot_b_verified = 1;
        slot->pending_verify = 0;
        module_slots_save();

        set_status(m, PKGST_VERIFIED);
        persist(m, PKGST_VERIFIED, 1);
        log_pkg(SEV_INFO, "PKG-ACT-003", "Verified package", m,
                "new slot verified - rollback-pkg still available");
        journal_pkg(JE_PKG_VERIFIED, m, 1, "PKGVER");
        return 1;
    }

    set_status(m, PKGST_FAILED);
    persist(m, PKGST_FAILED, 0);
    log_pkg(SEV_ERROR, "PKG-ACT-004", "Verification FAILED for package", m,
            "rollback-pkg to return to the known-good slot");
    journal_pkg(JE_PKG_FAILED, m, 0, "PKGFAI");
    return -2;
}

int pkg_rollback(const char *id)
{
    struct pkg_manifest *m = manifest_find(id);
    if (!m)
        return 0;

    enum pkg_status st = recovery_package_status(m);
    if (st != PKGST_APPLIED && st != PKGST_VERIFIED && st != PKGST_FAILED)
        return -1;

    struct module_slot *slot = module_slot_for(m->target_module);
    if (!slot || slot->rollback_slot == SLOT_NONE)
        return -1;

    /* Switch back to the untouched known-good slot. */
    slot->active_slot    = slot->rollback_slot;
    slot->rollback_slot  = SLOT_NONE;
    slot->pending_verify = 0;
    module_set_version(m->target_module, module_slot_active_version(slot));
    module_slots_save();

    set_status(m, PKGST_ROLLED_BACK);
    persist(m, PKGST_ROLLED_BACK, 1);
    log_pkg(SEV_INFO, "PKG-RB-001", "Rolled back package", m,
            "known-good slot active again");
    journal_pkg(JE_PKG_ROLLED_BACK, m, 1, "PKGRBK");
    return 1;
}
