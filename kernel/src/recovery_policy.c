/* recovery_policy.c — see recovery_policy.h. Keep this file SHORT: its
 * entire value is being small enough to audit at a glance. */
#include "recovery_policy.h"

enum policy_verdict policy_check(enum risk_level risk, int reversible)
{
    if (risk == RISK_HIGH)
        return POLICY_BLOCKED;      /* rule 1 */
    if (!reversible)
        return POLICY_BLOCKED;      /* rule 2 */
    return POLICY_NEEDS_APPROVAL;   /* rules 3 + 4 */
}

const char *policy_reason(enum risk_level risk, int reversible)
{
    if (risk == RISK_HIGH)
        return "high-risk actions are blocked by policy";
    if (!reversible)
        return "irreversible actions are blocked by policy";
    return "requires human approval";
}
