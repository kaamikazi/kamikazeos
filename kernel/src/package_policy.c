/* package_policy.c — see package_policy.h. Keep SHORT: auditability is
 * the security property. */
#include "package_policy.h"

const char *signature_status_name(enum signature_status s)
{
    switch (s) {
    case SIG_UNSIGNED:    return "unsigned";
    case SIG_PLACEHOLDER: return "signed_placeholder";
    case SIG_INVALID:     return "invalid";
    case SIG_TRUSTED:     return "trusted";
    }
    return "?";
}

enum pkg_verdict package_policy_check(enum signature_status sig,
                                      enum risk_level risk,
                                      int reversible, int checksum_ok)
{
    if (sig == SIG_UNSIGNED || sig == SIG_INVALID)
        return PKG_POLICY_BLOCKED;        /* rules 1 + 2 */
    if (risk == RISK_HIGH)
        return PKG_POLICY_BLOCKED;        /* rule 3 */
    if (!reversible)
        return PKG_POLICY_BLOCKED;        /* rule 4 */
    if (!checksum_ok)
        return PKG_POLICY_BLOCKED;        /* rule 5 */
    return PKG_POLICY_NEEDS_APPROVAL;     /* rules 6 + 7 */
}

const char *package_policy_reason(enum signature_status sig,
                                  enum risk_level risk,
                                  int reversible, int checksum_ok)
{
    if (sig == SIG_UNSIGNED)  return "unsigned packages are blocked";
    if (sig == SIG_INVALID)   return "invalid signature - blocked";
    if (risk == RISK_HIGH)    return "high-risk packages are blocked";
    if (!reversible)          return "irreversible packages are blocked in v1.2";
    if (!checksum_ok)         return "checksum verification failed - blocked";
    return "requires human approval";
}
