/* recovery_actions.c — the RecoveryEngine implementation.
 * Read recovery_actions.h first for the closed-loop design. */
#include "recovery_actions.h"
#include "errors.h"
#include "ghostattach.h"
#include "recovery_memory.h"
#include "recovery_policy.h"
#include "recovery_store.h"
#include "string.h"

/* ------------------------------------------------------------------ */
/* The fixed action catalog. THIS LIST IS THE RECOVERY SYSTEM'S ENTIRE  */
/* VOCABULARY — nothing outside it can ever be suggested or applied.    */
/* ------------------------------------------------------------------ */
#define ACTION_COUNT 5
static struct recovery_action actions[ACTION_COUNT] = {
    {
        .action_id        = "ACT-001",
        .name             = "quarantine_network_device",
        .description      = "Isolate the driverless network device so it stops generating errors",
        .target_component = "GhostAttach/network",
        .risk_level       = RISK_LOW,
        .reversible       = 1,
        .requires_approval= 1,
        .status           = ACTST_AVAILABLE,
        .suggest_hint     = "approve ACT-001 to isolate the driverless network device",
    },
    {
        .action_id        = "ACT-002",
        .name             = "quarantine_audio_device",
        .description      = "Isolate the audio device if its fallback driver misbehaves",
        .target_component = "GhostAttach/audio",
        .risk_level       = RISK_LOW,
        .reversible       = 1,
        .requires_approval= 1,
        .status           = ACTST_AVAILABLE,
        .suggest_hint     = "approve ACT-002 to isolate the audio device",
    },
    {
        .action_id        = "ACT-003",
        .name             = "block_unverified_storage_fallback",
        .description      = "Keep the unverified storage fallback driver blocked",
        .target_component = "GhostAttach/storage",
        .risk_level       = RISK_LOW,
        .reversible       = 1,
        .requires_approval= 0,    /* GhostAttach rule 3 already enforces it */
        .status           = ACTST_AVAILABLE,   /* set to APPLIED in init    */
        .suggest_hint     = "already enforced by GhostAttach policy",
    },
    {
        .action_id        = "ACT-004",
        .name             = "boot_safe_mode_next",
        .description      = "Boot into safe mode on the next restart (simulated flag)",
        .target_component = "kernel/recovery",
        .risk_level       = RISK_MEDIUM,
        .reversible       = 1,
        .requires_approval= 1,
        .status           = ACTST_AVAILABLE,
        .suggest_hint     = "approve ACT-004 to investigate isolated devices in safe mode",
    },
    {
        /* The canary: exists so the block path is exercised and visible.
           If this action EVER applies, the policy gate is broken. */
        .action_id        = "ACT-999",
        .name             = "install_unknown_driver",
        .description      = "Install an unverified unknown driver (FORBIDDEN)",
        .target_component = "GhostAttach/driver",
        .risk_level       = RISK_HIGH,
        .reversible       = 0,
        .requires_approval= 1,
        .status           = ACTST_AVAILABLE,   /* set to BLOCKED in init */
        .suggest_hint     = "never - unknown driver installation is not allowed",
    },
};

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */

const char *action_status_name(enum action_status s)
{
    switch (s) {
    case ACTST_AVAILABLE:   return "available";
    case ACTST_SUGGESTED:   return "suggested";
    case ACTST_APPROVED:    return "approved";
    case ACTST_APPLIED:     return "applied";
    case ACTST_ROLLED_BACK: return "rolled_back";
    case ACTST_BLOCKED:     return "blocked";
    }
    return "?";
}

static struct recovery_action *find_action(const char *id)
{
    if (!id || id[0] == '\0')
        return 0;
    for (int i = 0; i < ACTION_COUNT; i++)
        if (kstricmp(actions[i].action_id, id) == 0)
            return &actions[i];
    return 0;
}

/* Numeric byte for the durable journal: 1..4, 255 for the canary. */
static uint8_t act_byte(const struct recovery_action *a)
{
    if (kstrcmp(a->action_id, "ACT-999") == 0)
        return 255;
    return (uint8_t)(a->action_id[6] - '0');   /* "ACT-00N" -> N */
}

/* Append one durable journal record for an action event. */
static void journal_action(enum journal_event ev,
                           const struct recovery_action *a,
                           uint8_t result, const char *code6)
{
    recovery_journal_append(ev, act_byte(a), result,
                            (uint8_t)a->risk_level,
                            (uint8_t)a->reversible, code6);
}

/* Log "<verb> action ACT-xxx <name>" with a given code/severity/hint. */
static void log_action(enum err_severity sev, const char *code,
                       const char *component, const char *verb,
                       const struct recovery_action *a, const char *hint)
{
    char msg[ERR_MSG_MAX];
    kstrcpy(msg, verb, ERR_MSG_MAX);
    size_t len = kstrlen(msg);
    if (len < ERR_MSG_MAX - 1) { msg[len++] = ' '; msg[len] = '\0';
        kstrcpy(msg + len, a->action_id, ERR_MSG_MAX - len); }
    len = kstrlen(msg);
    if (len < ERR_MSG_MAX - 1) { msg[len++] = ' '; msg[len] = '\0';
        kstrcpy(msg + len, a->name, ERR_MSG_MAX - len); }
    error_log(sev, code, component, msg, hint);
}

/* ------------------------------------------------------------------ */
/* Apply / rollback — the ONLY code that changes system state.          */
/* ------------------------------------------------------------------ */

/* Returns 1 on success. Records prior state for rollback first. */
static int apply_action(struct recovery_action *a)
{
    if (kstrcmp(a->action_id, "ACT-001") == 0) {
        a->prior_status = ghostattach_get_status_by_type(DEV_NETWORK);
        ghostattach_set_status_by_type(DEV_NETWORK, DEVST_ISOLATED);
        /* Persist intent + prior state, so the quarantine (and the
           ability to roll it back) survives a reboot. */
        recovery_memory_mark_action_applied(a->action_id, a->prior_status);
        return 1;
    }
    if (kstrcmp(a->action_id, "ACT-002") == 0) {
        a->prior_status = ghostattach_get_status_by_type(DEV_AUDIO);
        ghostattach_set_status_by_type(DEV_AUDIO, DEVST_ISOLATED);
        recovery_memory_mark_action_applied(a->action_id, a->prior_status);
        return 1;
    }
    if (kstrcmp(a->action_id, "ACT-004") == 0) {
        /* v0.6: this is REAL now — the flag lives in NVRAM and the next
           boot reads it before scanning devices. */
        recovery_memory_set_safe_mode_next_boot(1);
        /* prior=0 is unused for ACT-004; this records last_action_id. */
        recovery_memory_mark_action_applied(a->action_id, 0);
        return 1;
    }
    return 0;   /* ACT-003 is enforced elsewhere; ACT-999 never gets here */
}

static int unapply_action(struct recovery_action *a)
{
    if (kstrcmp(a->action_id, "ACT-001") == 0) {
        /* True rollback: restore the RECORDED prior status, whatever it
           was — safer than hardcoding an assumed "missing_driver". */
        ghostattach_set_status_by_type(DEV_NETWORK, a->prior_status);
        recovery_memory_mark_action_rolled_back(a->action_id);
        return 1;
    }
    if (kstrcmp(a->action_id, "ACT-002") == 0) {
        ghostattach_set_status_by_type(DEV_AUDIO, a->prior_status);
        recovery_memory_mark_action_rolled_back(a->action_id);
        return 1;
    }
    if (kstrcmp(a->action_id, "ACT-004") == 0) {
        recovery_memory_set_safe_mode_next_boot(0);
        recovery_memory_mark_action_rolled_back(a->action_id);
        return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

void recovery_init(void)
{
    for (int i = 0; i < ACTION_COUNT; i++) {
        struct recovery_action *a = &actions[i];

        /* Ask the policy gate up front; blocked actions are visibly
           blocked from boot, not discovered at approval time. */
        if (policy_check(a->risk_level, a->reversible) == POLICY_BLOCKED) {
            a->status = ACTST_BLOCKED;
            log_action(SEV_WARNING, "REC-BLOCK-999", "RecoveryPolicy",
                       "Blocked high-risk action", a,
                       "unknown driver installation is not allowed");
        }
    }

    /* ACT-003 needs no approval: GhostAttach rule 3 already enforced it
       during the boot scan. Reflect reality in the catalog. */
    struct recovery_action *a3 = find_action("ACT-003");
    a3->status = ACTST_APPLIED;
    log_action(SEV_INFO, "REC-ACT-003", "RecoveryEngine",
               "Applied (policy-enforced)", a3,
               "no approval needed - enforced by GhostAttach rule 3");

    /*
     * RESTORE PERSISTED QUARANTINES (v0.6). The boot scan just rebuilt
     * device states from scratch; RecoveryMemory remembers what a human
     * approved before the reboot. Re-apply it — and resurrect the catalog
     * entry as APPLIED with its recorded prior state, so rollback still
     * works across the reboot.
     */
    const struct recovery_memory *m = recovery_memory_get();

    if (m->network_quarantined) {
        struct recovery_action *a = find_action("ACT-001");
        a->prior_status = (enum device_status)m->prior_network_status;
        a->status = ACTST_APPLIED;
        ghostattach_set_status_by_type(DEV_NETWORK, DEVST_ISOLATED);
        log_action(SEV_INFO, "REC-MEM-001", "RecoveryMemory",
                   "Restored persisted quarantine", a,
                   "approved before reboot - rollback ACT-001 to undo");
    }
    if (m->audio_quarantined) {
        struct recovery_action *a = find_action("ACT-002");
        a->prior_status = (enum device_status)m->prior_audio_status;
        a->status = ACTST_APPLIED;
        ghostattach_set_status_by_type(DEV_AUDIO, DEVST_ISOLATED);
        log_action(SEV_INFO, "REC-MEM-002", "RecoveryMemory",
                   "Restored persisted quarantine", a,
                   "approved before reboot - rollback ACT-002 to undo");
    }
    if (m->safe_mode_next_boot) {
        /* The flag is set but hasn't fired yet (scheduled this session
           or... scheduled then rebooted into safe mode clears it before
           we get here). Reflect it in the catalog. */
        find_action("ACT-004")->status = ACTST_APPLIED;
    }
}

void recovery_suggest_scan(void)
{
    const struct recovery_memory *m = recovery_memory_get();

    /* Rule 1: a driverless network device generates noise and has no
       function — suggest quarantining it. */
    if (ghostattach_get_status_by_type(DEV_NETWORK) == DEVST_MISSING_DRIVER) {
        struct recovery_action *a = find_action("ACT-001");
        if (a->status == ACTST_AVAILABLE) {
            a->status = ACTST_SUGGESTED;
            log_action(SEV_INFO, "REC-ACT-001", "RecoveryEngine",
                       "Suggested action", a, a->suggest_hint);
            journal_action(JE_SUGGESTED, a, 1, "REC001");
        }
    }

    /* Rule 2 (v0.6): the PREVIOUS boot ended in a panic — the strongest
       possible reason to offer a minimal-drivers boot for investigation. */
    if (m->prev_panic_detected && !m->safe_mode_active) {
        struct recovery_action *a = find_action("ACT-004");
        if (a->status == ACTST_AVAILABLE) {
            a->status = ACTST_SUGGESTED;
            log_action(SEV_WARNING, "BOOT-WARN-001", "RecoveryEngine",
                       "Previous boot ended in panic - suggested", a,
                       "approve ACT-004 to investigate in safe mode");
            journal_action(JE_SUGGESTED, a, 1, "REC001");
        }
    }

    /* Rule 3: something got isolated this boot — safe mode next boot
       would let a human investigate with minimal drivers loaded.
       (Skipped during a safe-mode boot: we're already there, and safe
       mode itself isolates devices on purpose.) */
    struct ghostattach_stats st;
    ghostattach_get_stats(&st);
    if (st.isolated > 0 && !m->safe_mode_active) {
        struct recovery_action *a = find_action("ACT-004");
        if (a->status == ACTST_AVAILABLE) {
            a->status = ACTST_SUGGESTED;
            log_action(SEV_INFO, "REC-ACT-001", "RecoveryEngine",
                       "Suggested action", a, a->suggest_hint);
            journal_action(JE_SUGGESTED, a, 1, "REC001");
        }
    }
}

int recovery_approve(const char *action_id)
{
    struct recovery_action *a = find_action(action_id);
    if (!a)
        return 0;   /* shell prints "unknown action" */

    /* THE GATE. Re-checked here even though init already marked blocked
       actions — defense in depth costs one comparison. */
    if (a->status == ACTST_BLOCKED ||
        policy_check(a->risk_level, a->reversible) == POLICY_BLOCKED) {
        log_action(SEV_WARNING, "REC-BLOCK-999", "RecoveryPolicy",
                   "Blocked high-risk action", a,
                   "unknown driver installation is not allowed");
        /* Durable evidence: someone TRIED. Repeated attempts are a
           pattern the analyzer watches for across boots. */
        journal_action(JE_BLOCKED, a, 0, "BLK999");
        return -1;  /* shell prints the block reason */
    }

    if (a->status == ACTST_APPLIED)
        return -2;  /* already applied */

    a->status = ACTST_APPROVED;
    log_action(SEV_INFO, "REC-ACT-002", "RecoveryEngine",
               "Approved action", a, "applying now");
    journal_action(JE_APPROVED, a, 1, "REC002");

    if (!apply_action(a)) {
        a->status = ACTST_AVAILABLE;
        log_action(SEV_ERROR, "REC-ACT-004", "RecoveryEngine",
                   "Apply FAILED for action", a, "state unchanged - report this bug");
        journal_action(JE_APPLIED, a, 0, "REC004");
        return -3;
    }

    a->status = ACTST_APPLIED;
    log_action(SEV_INFO, "REC-ACT-003", "RecoveryEngine",
               "Applied action", a, "reversible - use rollback to undo");
    journal_action(JE_APPLIED, a, 1, "REC003");
    return 1;
}

int recovery_rollback(const char *action_id)
{
    struct recovery_action *a = find_action(action_id);
    if (!a)
        return 0;
    if (a->status != ACTST_APPLIED || !a->reversible)
        return -1;  /* nothing applied to roll back (or irreversible) */
    if (a->requires_approval == 0)
        return -2;  /* ACT-003: policy-enforced, not user-undoable */

    if (!unapply_action(a))
        return -3;

    a->status = ACTST_ROLLED_BACK;
    log_action(SEV_INFO, "REC-RB-001", "RecoveryEngine",
               "Rolled back action", a, "prior state restored");
    journal_action(JE_ROLLED_BACK, a, 1, "RBK001");
    return 1;
}

uint32_t recovery_action_count(void) { return ACTION_COUNT; }

const struct recovery_action *recovery_action(uint32_t index)
{
    return (index < ACTION_COUNT) ? &actions[index] : 0;
}

int recovery_safe_mode_next(void)
{
    /* v0.6: the truth lives in NVRAM now, not a RAM flag. */
    return recovery_memory_get()->safe_mode_next_boot;
}
