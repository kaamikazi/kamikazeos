/*
 * package_manifest.h — disk-backed package manifests + the TrustAnchor.
 *
 * A MANIFEST describes one allowed change: what it targets, which version
 * it upgrades from/to, how risky it is, who vouches for it (trust anchor),
 * and what kind of payload it carries. Manifests live ON DISK (LBA 32–47,
 * one per sector, checksummed) so the update pipeline shape is real:
 * media in, verification, policy, then a human decision.
 *
 * THE TRUST ANCHOR is the root of update trust: a built-in identity that
 * every manifest must reference. v1.0 HONESTY LABEL: signature
 * verification is SIMULATED — the manifest carries a trust_anchor_id and
 * a signature status, and the kernel enforces the policy exactly as if
 * they were cryptographic (unknown anchor or unsigned = blocked), but no
 * real Ed25519 math runs yet. Real crypto is Phase 11; the ENFORCEMENT
 * PATH it will plug into is fully built and exercised today.
 *
 * What a payload may be (NO other kind exists):
 *   metadata_only            change version metadata only
 *   module_slot_switch_sim   switch a module's A/B slot (metadata-level)
 *   blocked_driver_install   recognized and ALWAYS refused (the canary)
 * There is no payload type that executes code, and no API that loads
 * anything but these fixed-shape manifest records.
 */
#ifndef KMZ_PACKAGE_MANIFEST_H
#define KMZ_PACKAGE_MANIFEST_H

#include <stdint.h>
#include "driver.h"            /* enum risk_level */
#include "module_versions.h"   /* enum module_id  */
#include "package_policy.h"    /* enum signature_status */

/* The built-in trust anchor (v1.1: simulated identity, real enforcement). */
#define TRUST_ANCHOR_ID          1
#define TRUST_ANCHOR_NAME        "KMZ-ROOT-1"
#define TRUST_ANCHOR_FINGERPRINT "SIM:4B5A:1000:C0DE:FACE:2026"

enum payload_type {
    PAYLOAD_METADATA_ONLY = 0,
    PAYLOAD_SLOT_SWITCH_SIM,
    PAYLOAD_BLOCKED_DRIVER_INSTALL,
    PAYLOAD_UNKNOWN,
};

#define PM_ID_MAX     10
#define PM_NAME_MAX   32
#define PM_DESC_MAX   48
#define PM_TARGET_MAX 20
#define PM_VER_MAX    8
#define MANIFEST_MAX  8        /* fits comfortably in LBA 32..47 */

struct pkg_manifest {
    char               package_id[PM_ID_MAX];
    char               name[PM_NAME_MAX];
    char               description[PM_DESC_MAX];
    char               target_component[PM_TARGET_MAX];
    enum module_id     target_module;     /* which slot table entry      */
    char               from_version[PM_VER_MAX];
    char               to_version[PM_VER_MAX];
    enum risk_level    risk_level;
    uint8_t            reversible;
    uint8_t            requires_approval;
    uint8_t            checksum;          /* expected (from media)       */
    enum signature_status signature_status;
    uint8_t            trust_anchor_id;
    uint8_t            manifest_version;
    enum payload_type  payload_type;

    /* computed at load time */
    uint8_t            checksum_ok;
};

/* Load manifests: disk sectors first; if none are valid, fall back to
   the built-in seed set (and remember which source was used). */
void package_manifest_init(void);

uint32_t                  manifest_count(void);
struct pkg_manifest      *manifest_get(uint32_t index);
struct pkg_manifest      *manifest_find(const char *package_id);
int  manifest_source_is_disk(void);     /* 1 = loaded from disk sectors */
const char *manifest_source_name(void);  /* disk_imported or built_in_seed */

/* Write the BUILT-IN SEED manifests to the disk sectors (seed-manifests
   confirm). Only the safe fixed set is ever written; returns count. */
int  manifest_seed_to_disk(void);

/* Full verification chain. Returns 1 = passes; on failure *reason names
   the first failed rule (for logs, shell, and the analyzer). */
int  manifest_verify(const struct pkg_manifest *m, const char **reason);
int  manifest_checksum_valid(const struct pkg_manifest *m);
int  manifest_trust_valid(const struct pkg_manifest *m);
const char *manifest_policy_result(const struct pkg_manifest *m);
const char *manifest_block_reason(const struct pkg_manifest *m);

const char *payload_type_name(enum payload_type t);

#endif /* KMZ_PACKAGE_MANIFEST_H */
