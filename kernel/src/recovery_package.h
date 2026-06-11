/*
 * recovery_package.h — RecoveryPackage v1.0: the controlled update
 * pipeline over manifests, A/B slots, and persistent state.
 *
 * THE PIPELINE (every step human-driven, journaled, and PERSISTENT):
 *
 *   manifest (disk, trust-verified)
 *      -> approve-pkg   policy + verification gate, approval persisted
 *      -> apply-pkg     switch the module's A/B slot to the new version,
 *                       old slot untouched = guaranteed rollback,
 *                       pending_verify set (survives reboot!)
 *      -> verify-pkg    version + health checks; new slot marked verified
 *      -> rollback-pkg  switch back to the known-good slot
 *
 * v1.0 honesty: slots hold METADATA (version strings), not module
 * binaries — no code is patched or loaded. Signature verification is
 * SIMULATED (trust anchor id + status fields, real enforcement, no real
 * crypto yet). Everything else — disk manifests, persistent state,
 * checksums, policy rejection, the durable journal — is real.
 */
#ifndef KMZ_RECOVERY_PACKAGE_H
#define KMZ_RECOVERY_PACKAGE_H

#include <stdint.h>
#include "package_manifest.h"
#include "package_state.h"

enum pkg_status {
    PKGST_AVAILABLE = 0,
    PKGST_SUGGESTED,
    PKGST_APPROVED,
    PKGST_APPLIED,        /* slot switched, verification pending */
    PKGST_VERIFIED,
    PKGST_FAILED,
    PKGST_ROLLED_BACK,
    PKGST_BLOCKED,
};

/* Init order matters: manifests -> slots -> states -> hydrate statuses.
   Logs PKG-PENDING-VERIFY if a previous boot applied without verifying. */
void recovery_package_init(void);

/* Lifecycle verbs (ids case-insensitive).
   Return: 1 ok, 0 unknown id, negative = refused (shell explains). */
int pkg_approve(const char *id);    /* -1 blocked                        */
int pkg_apply(const char *id);      /* -1 blocked, -2 not approved,
                                       -3 from_version mismatch          */
int pkg_verify(const char *id);     /* -1 not applied, -2 FAILED         */
int pkg_rollback(const char *id);   /* -1 nothing to roll back           */

/* Current status of a manifest's package (hydrated from persistence). */
enum pkg_status recovery_package_status(const struct pkg_manifest *m);

const char *pkg_status_name(enum pkg_status s);

#endif /* KMZ_RECOVERY_PACKAGE_H */
