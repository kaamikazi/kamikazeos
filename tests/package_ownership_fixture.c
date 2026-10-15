/* Execute actual ownership commits with deterministic persistence failures. */
#include "../kernel/src/recovery_package.c"

static struct pkg_pstate records[3];
static struct pkg_pstate durable[3];
static struct pkg_manifest current_manifest;
static struct module_slot running;
static int fail_save;

uint32_t package_state_count(void) { return 3; }
const struct pkg_pstate *package_state_get(uint32_t i) { return &records[i]; }
struct pkg_pstate *package_state_find(const char *id)
{
    for (int i = 0; i < 3; i++)
        if (kstrcmp(id, records[i].package_id) == 0)
            return &records[i];
    return 0;
}
struct pkg_pstate *package_state_ensure(const char *id) { return package_state_find(id); }
int package_state_save(void)
{
    if (fail_save)
        return 0;
    memcpy(durable, records, sizeof(records));
    return 1;
}
struct pkg_manifest *manifest_find(const char *id)
{
    return kstrcmp(id, current_manifest.package_id) == 0 ? &current_manifest : 0;
}
uint32_t manifest_identity(const struct pkg_manifest *m) { (void)m; return 42; }
struct module_slot *module_slot_for(enum module_id id) { (void)id; return &running; }
const char *module_slot_active_version(const struct module_slot *s)
{
    return s->active_slot == SLOT_A ? s->slot_a_version : s->slot_b_version;
}
void error_log(enum err_severity severity, const char *code, const char *component,
               const char *message, const char *hint)
{
    (void)severity; (void)code; (void)component; (void)message; (void)hint;
}

static void prepare(void)
{
    kmemset(records, 0, sizeof(records));
    kmemset(&current_manifest, 0, sizeof(current_manifest));
    kmemset(&running, 0, sizeof(running));
    fail_save = 0;
    const char *ids[] = { "PKG-202", "PKG-203", "PKG-301" };
    for (int i = 0; i < 3; i++) {
        kstrcpy(records[i].package_id, ids[i], PM_ID_MAX);
        records[i].extension_valid = 1;
        records[i].manifest_identity = 42;
        records[i].target_module = i == 2 ? MOD_AI_RULES : MOD_GHOSTATTACH;
        records[i].slot_before_apply = i == 0 ? SLOT_B : SLOT_A;
        records[i].slot_after_apply = i == 0 ? SLOT_A : SLOT_B;
        records[i].transaction_phase = i == 1 ? PST_TX_APPLY_STAGED : PST_TX_COMMITTED;
        records[i].status_before_transaction = PKGST_APPROVED;
        state_set_status(&records[i], i == 1 ? PKGST_APPROVED : PKGST_VERIFIED);
        kstrcpy(records[i].from_version, i == 0 ? "0.4" : "0.3", PM_VER_MAX);
        kstrcpy(records[i].to_version, i == 0 ? "0.3" : "0.4", PM_VER_MAX);
    }
    kstrcpy(current_manifest.package_id, "PKG-203", PM_ID_MAX);
    kstrcpy(current_manifest.from_version, "0.3", PM_VER_MAX);
    kstrcpy(current_manifest.to_version, "0.4", PM_VER_MAX);
    current_manifest.target_module = MOD_GHOSTATTACH;
    running.active_slot = SLOT_B;
    running.rollback_slot = SLOT_A;
    running.pending_verify = 1;
    kstrcpy(running.slot_a_version, "0.3", SLOT_VER_MAX);
    kstrcpy(running.slot_b_version, "0.4", SLOT_VER_MAX);
    memcpy(durable, records, sizeof(records));
}

int test_owner_commit_retires_only_same_module(void)
{
    prepare();
    if (!commit_state(&current_manifest, PKGST_APPLIED, 1))
        return 0;
    return records[0].slot_before_apply == SLOT_NONE && records[0].verified &&
           records[1].applied && records[1].slot_before_apply == SLOT_A &&
           records[2].slot_before_apply == SLOT_A &&
           durable[0].slot_before_apply == SLOT_NONE && durable[1].applied;
}

int test_failed_commit_and_reconciliation_preserve_staged_intent(void)
{
    prepare();
    fail_save = 1;
    if (commit_state(&current_manifest, PKGST_APPLIED, 1) ||
        records[0].slot_before_apply != SLOT_B ||
        records[1].transaction_phase != PST_TX_APPLY_STAGED ||
        durable[0].slot_before_apply != SLOT_B)
        return 0;
    recover_interrupted_transactions();
    if (records[0].slot_before_apply != SLOT_B ||
        records[1].transaction_phase != PST_TX_APPLY_STAGED)
        return 0;
    /* A later successful reconciliation publishes owner + revocations together. */
    fail_save = 0;
    memcpy(records, durable, sizeof(records));
    recover_interrupted_transactions();
    return records[0].slot_before_apply == SLOT_NONE && records[1].applied &&
           records[1].transaction_phase == PST_TX_COMMITTED &&
           durable[0].slot_before_apply == SLOT_NONE && durable[1].applied;
}
