/*
 * package_policy.h - the deterministic gate for recovery packages.
 *
 * Like recovery_policy.h, this file's entire value is being SHORT and
 * auditable. The package rules:
 *   1. unsigned signature .......... BLOCKED
 *   2. invalid signature ........... BLOCKED
 *   3. high-risk ................... BLOCKED
 *   4. irreversible ................ BLOCKED
 *   5. checksum mismatch ........... BLOCKED
 *   6. medium risk ................. human approval required
 *   7. low risk .................... human approval required
 *   8. imported manifests .......... allowed only after independent kernel
 *                                    validation
 */
#ifndef KMZ_PACKAGE_POLICY_H
#define KMZ_PACKAGE_POLICY_H

#include "driver.h"   /* enum risk_level */

enum signature_status {
    SIG_UNSIGNED = 0,
    SIG_PLACEHOLDER,    /* simulated signature status; not real crypto */
    SIG_INVALID,
    SIG_TRUSTED,        /* reserved for real verification */
};

enum pkg_verdict {
    PKG_POLICY_NEEDS_APPROVAL = 0,
    PKG_POLICY_BLOCKED,
};

enum pkg_verdict package_policy_check(enum signature_status sig,
                                      enum risk_level risk,
                                      int reversible, int checksum_ok);
const char *package_policy_reason(enum signature_status sig,
                                  enum risk_level risk,
                                  int reversible, int checksum_ok);
const char *signature_status_name(enum signature_status s);

#endif /* KMZ_PACKAGE_POLICY_H */
