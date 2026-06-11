/*
 * recovery_actions.h — the RecoveryEngine: KamikazeOS's human-approved
 * repair layer.
 *
 * THE CLOSED LOOP (v0.5):
 *   kernel logs an issue  ->  suggestion rules (and the host-side AI
 *   analyzer) point at an action ID from THIS FIXED CATALOG  ->  a human
 *   types `approve ACT-xxx` in the shell  ->  the policy gate judges it
 *   ->  the engine applies a SIMULATED, REVERSIBLE state change  ->
 *   `rollback ACT-xxx` undoes it. Every step is logged with a REC-* code.
 *
 * Why a fixed catalog? Because "the AI can do anything it can describe"
 * is exactly the failure mode we're designing against. The catalog is the
 * complete list of things the recovery system can EVER do; suggesting an
 * action not in it is impossible by construction, in both the kernel
 * rules and the host analyzer.
 *
 * v0.5 honesty: "applying" an action changes registry state (device
 * quarantined, safe-mode flag set) — real bookkeeping, simulated hardware
 * effect. Persistence across reboots arrives with storage in Phase 6.
 */
#ifndef KMZ_RECOVERY_ACTIONS_H
#define KMZ_RECOVERY_ACTIONS_H

#include <stdint.h>
#include "device.h"
#include "driver.h"     /* enum risk_level */

enum action_status {
    ACTST_AVAILABLE = 0,   /* in the catalog, nothing suggests it now   */
    ACTST_SUGGESTED,       /* suggestion rules matched a live issue     */
    ACTST_APPROVED,        /* human said yes; about to apply            */
    ACTST_APPLIED,         /* state change is in effect                 */
    ACTST_ROLLED_BACK,     /* was applied, then undone                  */
    ACTST_BLOCKED,         /* policy says never                        */
};

struct recovery_action {
    const char        *action_id;        /* "ACT-001"                     */
    const char        *name;             /* "quarantine_network_device"   */
    const char        *description;
    const char        *target_component; /* "GhostAttach/network"         */
    enum risk_level    risk_level;
    int                reversible;
    int                requires_approval;
    enum action_status status;

    /* What the suggestion log's hint should say (becomes the AI hint). */
    const char        *suggest_hint;

    /* Rollback bookkeeping: device status before we changed it. */
    enum device_status prior_status;
};

/* Initialize catalog states (marks ACT-999 blocked, ACT-003 applied). */
void recovery_init(void);

/* Run the suggestion rules against current GhostAttach state and log
   REC-ACT-001 events. Call after ghostattach_scan(). */
void recovery_suggest_scan(void);

/* approve/rollback an action by its "ACT-xxx" id (case-insensitive).
   All outcomes are logged AND printed by the shell; return 1 on success. */
int recovery_approve(const char *action_id);
int recovery_rollback(const char *action_id);

/* Read-only access for shell + report. */
uint32_t                      recovery_action_count(void);
const struct recovery_action *recovery_action(uint32_t index);

/* The simulated "boot into safe mode next time" flag (ACT-004). */
int recovery_safe_mode_next(void);

const char *action_status_name(enum action_status s);

#endif /* KMZ_RECOVERY_ACTIONS_H */
