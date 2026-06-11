/*
 * recovery_policy.h — the deterministic gate every recovery action passes.
 *
 * This is the most security-critical 40 lines in KamikazeOS, so it is
 * DUMB ON PURPOSE: a handful of fixed rules, no state, no learning, no
 * exceptions, auditable in one read. The AI analyzer, the shell, the
 * future update system — everyone goes through the same gate, and nothing
 * in software can widen it at runtime.
 *
 * The rules (v0.5):
 *   1. HIGH-risk actions ........... BLOCKED, not approvable
 *   2. Irreversible actions ........ BLOCKED, not approvable
 *   3. MEDIUM-risk actions ......... require human approval
 *   4. LOW-risk actions ............ require human approval too
 *                                    (auto-allow may come later, opt-in)
 *   5. Unverified driver install ... BLOCKED (also rule 1+2 by definition)
 *   6. AI direct repair ............ BLOCKED structurally: the AI has no
 *                                    code path that reaches apply() at all
 */
#ifndef KMZ_RECOVERY_POLICY_H
#define KMZ_RECOVERY_POLICY_H

#include "driver.h"     /* enum risk_level */

enum policy_verdict {
    POLICY_NEEDS_APPROVAL = 0,   /* allowed once a human approves        */
    POLICY_BLOCKED,              /* never allowed, not even with approval */
};

/* Judge one action by its safety facts. Pure function, no side effects. */
enum policy_verdict policy_check(enum risk_level risk, int reversible);

/* One-line human explanation of the verdict, for shell + logs. */
const char *policy_reason(enum risk_level risk, int reversible);

#endif /* KMZ_RECOVERY_POLICY_H */
