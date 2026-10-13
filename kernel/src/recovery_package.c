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

static void log_pkg_class(enum err_severity sev, enum health_class cls,
                          const char *code, const char *verb,
                          const struct pkg_manifest *m, const char *hint)
{
    char msg[ERR_MSG_MAX];
    kstrcpy(msg, verb, ERR_MSG_MAX);
    size_t len = kstrlen(msg);
    if (len < ERR_MSG_MAX - 1) { msg[len++] = ' '; msg[len] = '\0';
        kstrcpy(msg + len, m->package_id, ERR_MSG_MAX - len); }
    len = kstrlen(msg);
    if (len < ERR_MSG_MAX - 1) { msg[len++] = ' '; msg[len] = '\0';
        kstrcpy(msg + len, m->name, ERR_MSG_MAX - len); }
    error_log_class(sev, cls, code, "RecoveryPackage", msg, hint);
}

static void log_pkg(enum err_severity sev, const char *code,
                    const char *verb, const struct pkg_manifest *m,
                    const char *hint)
{
    log_pkg_class(sev, HEALTH_PRODUCTION, code, verb, m, hint);
}

static void journal_pkg(enum journal_event ev, const struct pkg_manifest *m,
                        uint8_t result, const char *code6)
{
    recovery_journal_append(ev, pkg_byte(m), result,
                            (uint8_t)m->risk_level, m->reversible, code6);
}

static void state_set_status(struct pkg_pstate *s, enum pkg_status st)
{
    s->status = (uint8_t)st;
    s->approved = s->applied = s->verified = 0;
    s->failed = s->rolled_back = 0;
    if (st == PKGST_APPROVED) s->approved = 1;
    if (st == PKGST_APPLIED) s->applied = 1;
    if (st == PKGST_VERIFIED) s->verified = 1;
    if (st == PKGST_FAILED) s->failed = 1;
    if (st == PKGST_ROLLED_BACK) s->rolled_back = 1;
}

static void bind_state(struct pkg_pstate *s, const struct pkg_manifest *m)
{
    kstrcpy(s->from_version, m->from_version, PM_VER_MAX);
    kstrcpy(s->to_version, m->to_version, PM_VER_MAX);
    s->manifest_identity = manifest_identity(m);
    s->target_module = (uint8_t)m->target_module;
    s->extension_valid = 1;
}

static int binding_matches(const struct pkg_pstate *s,
                           const struct pkg_manifest *m)
{
    return s->extension_valid && s->manifest_identity == manifest_identity(m) &&
           s->target_module == (uint8_t)m->target_module &&
           kstrcmp(s->from_version, m->from_version) == 0 &&
           kstrcmp(s->to_version, m->to_version) == 0;
}

static int commit_state(const struct pkg_manifest *m, enum pkg_status st,
                        int result_code)
{
    struct pkg_pstate *s = package_state_ensure(m->package_id);
    if (!s)
        return 0;
    struct pkg_pstate previous = *s;
    bind_state(s, m);
    state_set_status(s, st);
    s->last_result_code = (uint8_t)(result_code & 0xFF);
    s->transaction_phase = PST_TX_COMMITTED;
    s->status_before_transaction = (uint8_t)st;
    if (!package_state_save()) {
        *s = previous;
        return 0;
    }
    return 1;
}

static int stage_transaction(const struct pkg_manifest *m, uint8_t phase,
                             uint8_t slot_before, uint8_t slot_after,
                             uint8_t criticals, uint8_t health)
{
    struct pkg_pstate *s = package_state_ensure(m->package_id);
    if (!s)
        return 0;
    struct pkg_pstate previous = *s;
    bind_state(s, m);
    s->status_before_transaction = s->status;
    s->transaction_phase = phase;
    s->slot_before_apply = slot_before;
    s->slot_after_apply = slot_after;
    if (phase == PST_TX_APPLY_STAGED) {
        s->criticals_at_apply = criticals;
        s->health_at_apply = health;
    }
    if (!package_state_save()) {
        *s = previous;
        return 0;
    }
    return 1;
}

static void recover_interrupted_transactions(void)
{
    for (uint32_t i = 0; i < package_state_count(); i++) {
        const struct pkg_pstate *view = package_state_get(i);
        if (!view || !view->extension_valid ||
            view->transaction_phase <= PST_TX_COMMITTED)
            continue;

        struct pkg_pstate *s = package_state_find(view->package_id);
        struct pkg_manifest *m = manifest_find(view->package_id);
        struct module_slot *slot = m ? module_slot_for(m->target_module) : 0;
        enum pkg_status recovered = PKGST_FAILED;
        const char *code = "PST-RECOVER-FAILED";
        const char *message = "Interrupted package transaction is inconsistent";

        if (m && binding_matches(s, m) && slot) {
            if (s->transaction_phase == PST_TX_APPLY_STAGED) {
                if (slot->active_slot == s->slot_after_apply &&
                    slot->pending_verify &&
                    kstrcmp(module_slot_active_version(slot), s->to_version) == 0) {
                    recovered = PKGST_APPLIED;
                    code = "PST-RECOVER-APPLY";
                    message = "Recovered committed slot switch from staged state";
                } else if (slot->active_slot == s->slot_before_apply &&
                           kstrcmp(module_slot_active_version(slot), s->from_version) == 0) {
                    recovered = (enum pkg_status)s->status_before_transaction;
                    code = "PST-RECOVER-ABORT";
                    message = "Discarded staged apply before slot commit";
                }
            } else if (s->transaction_phase == PST_TX_VERIFY_STAGED) {
                int active_verified = slot->active_slot == SLOT_A
                    ? slot->slot_a_verified : slot->slot_b_verified;
                if (!slot->pending_verify && active_verified)
                    recovered = PKGST_VERIFIED;
                else if (slot->pending_verify)
                    recovered = PKGST_APPLIED;
            } else if (s->transaction_phase == PST_TX_ROLLBACK_STAGED) {
                if (slot->active_slot == s->slot_before_apply &&
                    slot->rollback_slot == SLOT_NONE)
                    recovered = PKGST_ROLLED_BACK;
                else if (slot->active_slot == s->slot_after_apply)
                    recovered = (enum pkg_status)s->status_before_transaction;
            }
        }

        state_set_status(s, recovered);
        s->transaction_phase = PST_TX_COMMITTED;
        s->status_before_transaction = (uint8_t)recovered;
        if (!package_state_save())
            error_log(SEV_ERROR, "PST-RECOVER-SAVE", "PackageState",
                      "Recovered transaction state could not be persisted",
                      "reboot will retry transaction reconciliation");
        error_log(recovered == PKGST_FAILED ? SEV_ERROR : SEV_WARNING,
                  code, "PackageState", message,
                  "slot metadata was inspected but not automatically changed");
    }
}

static int migrate_legacy_state(struct pkg_pstate *s,
                                const struct pkg_manifest *m)
{
    if (!s || s->extension_valid ||
        kstrcmp(s->from_version, m->from_version) != 0 ||
        kstrcmp(s->to_version, m->to_version) != 0)
        return 0;
    if (s->status != PKGST_APPLIED && s->status != PKGST_VERIFIED &&
        s->status != PKGST_FAILED)
        return 0;
    struct module_slot *slot = module_slot_for(m->target_module);
    if (!slot || kstrcmp(module_slot_active_version(slot), m->to_version) != 0)
        return 0;
    bind_state(s, m);
    s->transaction_phase = PST_TX_COMMITTED;
    s->status_before_transaction = s->status;
    if (!package_state_save())
        return 0;
    error_log(SEV_INFO, "PST-MIGRATE-001", "PackageState",
              "Legacy active package state bound to current manifest",
              "LBA layout unchanged transaction sidecar added");
    return 1;
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
    recover_interrupted_transactions();

    for (uint32_t i = 0; i < manifest_count(); i++) {
        struct pkg_manifest *m = manifest_get(i);
        statuses[i] = PKGST_AVAILABLE;

        /* Verification chain first: failures are visibly blocked. */
        const char *reason;
        if (!manifest_verify(m, &reason)) {
            statuses[i] = PKGST_BLOCKED;
            /* The compiled-in canary (built-in seed, in RAM or as an exact
               disk copy) being blocked is the policy working. A blocked
               IMPORTED manifest is a real event and costs health. */
            log_pkg_class(SEV_WARNING,
                          m->origin == MANIFEST_ORIGIN_DISK_IMPORT
                              ? HEALTH_PRODUCTION : HEALTH_EXPECTED_CANARY,
                          "PKG-BLOCK-001", "Blocked package", m, reason);
            continue;
        }

        /* Hydrate persisted lifecycle state (survives reboots). */
        struct pkg_pstate *s = package_state_find(m->package_id);
        if (s && !s->extension_valid)
            migrate_legacy_state(s, m);
        if (s && binding_matches(s, m) &&
            s->transaction_phase == PST_TX_COMMITTED &&
            s->status <= PKGST_BLOCKED) {
            statuses[i] = (enum pkg_status)s->status;
            continue;
        }
        if (s && s->status == PKGST_APPROVED && !binding_matches(s, m))
            error_log(SEV_WARNING, "PST-BIND-001", "PackageState",
                      "Unbound package approval ignored",
                      "review and approve the current manifest again");

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
        log_pkg_class(SEV_WARNING, HEALTH_OPERATOR_INPUT,
                      "PKG-BLOCK-001", "Blocked package", m, reason);
        journal_pkg(JE_PKG_BLOCKED, m, 0, "PKGBLK");
        return -1;
    }

    if (!commit_state(m, PKGST_APPROVED, 1))
        return -4;
    set_status(m, PKGST_APPROVED);
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
    /* An unverified (or failed) update is still pending on this module.
       Applying over it would overwrite the inactive slot — the ONLY
       verified known-good version — and leave nothing to roll back to. */
    if (slot->pending_verify) {
        log_pkg_class(SEV_WARNING, HEALTH_OPERATOR_INPUT, "PKG-REJECT-002",
                "Apply refused - pending verification on module of", m,
                "verify-pkg or rollback-pkg the previous update first");
        return -6;
    }

    /* THE A/B SWITCH: stage the new version in the INACTIVE slot, flip
       the active pointer, keep the old slot untouched as the way back. */
    uint8_t old_slot = slot->active_slot;
    uint8_t new_slot = (old_slot == SLOT_A) ? SLOT_B : SLOT_A;
    struct health_breakdown hb;
    boot_snapshot_health_now(&hb);
    uint32_t crit_now = error_health_count(HEALTH_PRODUCTION, SEV_ERROR) +
                        error_health_count(HEALTH_PRODUCTION, SEV_PANIC);
    uint8_t criticals = (uint8_t)(crit_now > 255u ? 255u : crit_now);
    if (!stage_transaction(m, PST_TX_APPLY_STAGED, old_slot, new_slot,
                           criticals, (uint8_t)hb.score))
        return -4;

    struct module_slot previous_slot = *slot;
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
    if (!module_slots_save()) {
        *slot = previous_slot;
        module_set_version(m->target_module, module_slot_active_version(slot));
        commit_state(m, PKGST_APPROVED, 0);
        return -4;
    }
    set_status(m, PKGST_APPLIED);
    if (!commit_state(m, PKGST_APPLIED, 1)) {
        error_log(SEV_WARNING, "PST-COMMIT-PENDING", "PackageState",
                  "Slot switched but package commit remains staged",
                  "reboot will reconcile the staged transaction");
        return -4;
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
    uint32_t criticals_now = error_health_count(HEALTH_PRODUCTION, SEV_ERROR) +
                             error_health_count(HEALTH_PRODUCTION, SEV_PANIC);

    int slot_ok    = (slot->active_slot == s->slot_after_apply);
    int version_ok = (kstrcmp(module_slot_active_version(slot),
                              m->to_version) == 0);
    int crit_ok    = (criticals_now <= s->criticals_at_apply);
    int health_ok  = (hb.score >= (int)s->health_at_apply - 20);

    if (slot_ok && version_ok && crit_ok && health_ok) {
        if (!stage_transaction(m, PST_TX_VERIFY_STAGED,
                               s->slot_before_apply, s->slot_after_apply,
                               s->criticals_at_apply, s->health_at_apply))
            return -4;
        struct module_slot previous_slot = *slot;
        if (slot->active_slot == SLOT_A)
            slot->slot_a_verified = 1;
        else
            slot->slot_b_verified = 1;
        slot->pending_verify = 0;
        if (!module_slots_save()) {
            *slot = previous_slot;
            commit_state(m, PKGST_APPLIED, 0);
            return -4;
        }

        set_status(m, PKGST_VERIFIED);
        if (!commit_state(m, PKGST_VERIFIED, 1))
            return -4;
        log_pkg(SEV_INFO, "PKG-ACT-003", "Verified package", m,
                "new slot verified - rollback-pkg still available");
        journal_pkg(JE_PKG_VERIFIED, m, 1, "PKGVER");
        return 1;
    }

    set_status(m, PKGST_FAILED);
    if (!commit_state(m, PKGST_FAILED, 0))
        return -4;
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
    struct pkg_pstate *s = package_state_find(m->package_id);
    if (!slot || !s || !binding_matches(s, m))
        return -1;

    /* Ownership: roll back only the update that is ACTUALLY running. A
       later package for the same module may have switched slots since;
       its rollback pointer then belongs to it, not to this package. */
    const char *stale = 0;
    if (slot->active_slot != s->slot_after_apply ||
        kstrcmp(module_slot_active_version(slot), m->to_version) != 0)
        stale = "module no longer runs this package - another update owns it";
    else if (slot->rollback_slot == SLOT_NONE ||
             slot->rollback_slot != s->slot_before_apply)
        stale = "no rollback slot recorded for this package's apply";
    else if (kstrcmp(slot->rollback_slot == SLOT_A ? slot->slot_a_version
                                                   : slot->slot_b_version,
                     m->from_version) != 0)
        stale = "known-good from_version is no longer in the rollback slot";
    if (stale) {
        log_pkg_class(SEV_WARNING, HEALTH_OPERATOR_INPUT, "PKG-REJECT-001",
                      "Stale rollback refused", m, stale);
        journal_pkg(JE_PKG_ROLLED_BACK, m, 0, "PKGRBX");
        return -5;
    }

    if (!stage_transaction(m, PST_TX_ROLLBACK_STAGED,
                           s->slot_before_apply, s->slot_after_apply,
                           s->criticals_at_apply, s->health_at_apply))
        return -4;

    /* Switch back to the untouched known-good slot. */
    struct module_slot previous_slot = *slot;
    slot->active_slot    = slot->rollback_slot;
    slot->rollback_slot  = SLOT_NONE;
    slot->pending_verify = 0;
    module_set_version(m->target_module, module_slot_active_version(slot));
    if (!module_slots_save()) {
        *slot = previous_slot;
        module_set_version(m->target_module, module_slot_active_version(slot));
        commit_state(m, st, 0);
        return -4;
    }

    set_status(m, PKGST_ROLLED_BACK);
    if (!commit_state(m, PKGST_ROLLED_BACK, 1))
        return -4;
    log_pkg(SEV_INFO, "PKG-RB-001", "Rolled back package", m,
            "known-good slot active again");
    journal_pkg(JE_PKG_ROLLED_BACK, m, 1, "PKGRBK");
    return 1;
}
