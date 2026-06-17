/* package_manifest.c — manifest storage, loading, and verification.
 * Read package_manifest.h first: trust anchor + honesty labels live there. */
#include "package_manifest.h"
#include "ata.h"
#include "errors.h"
#include "string.h"

#define LBA_MANIFESTS 32
/* sector layout offsets */
#define MF_MAGIC0 'K'
#define MF_MAGIC1 'M'
#define MF_MAGIC2 'F'
#define MF_MAGIC3 'T'
#define MF_FMT_VERSION 1
#define O_MAGIC    0     /* 4 bytes */
#define O_FMTVER   4
#define O_ID       5     /* PM_ID_MAX     */
#define O_NAME     (O_ID + PM_ID_MAX)
#define O_DESC     (O_NAME + PM_NAME_MAX)
#define O_TARGET   (O_DESC + PM_DESC_MAX)
#define O_FROM     (O_TARGET + PM_TARGET_MAX)
#define O_TO       (O_FROM + PM_VER_MAX)
#define O_RISK     (O_TO + PM_VER_MAX)
#define O_REV      (O_RISK + 1)
#define O_REQ      (O_REV + 1)
#define O_SIG      (O_REQ + 1)
#define O_ANCHOR   (O_SIG + 1)
#define O_MVER     (O_ANCHOR + 1)
#define O_PAYLOAD  (O_MVER + 1)
#define O_MODULE   (O_PAYLOAD + 1)
#define O_CHECKSUM (O_MODULE + 1)
#define O_SIG_PRESENT (O_CHECKSUM + 1)
#define O_SIG_ALG     (O_SIG_PRESENT + 1)
#define O_SIG_KEY_HASH     (O_SIG_ALG + 1)
#define O_SIG_PAYLOAD_HASH (O_SIG_KEY_HASH + 17)
#define SIG_ALG_ED25519 1
#define O_END      (O_SIG_PAYLOAD_HASH + 17)

static struct pkg_manifest manifests[MANIFEST_MAX];
static uint32_t count;
static int from_disk;

const char *payload_type_name(enum payload_type t)
{
    switch (t) {
    case PAYLOAD_METADATA_ONLY:          return "metadata_only";
    case PAYLOAD_SLOT_SWITCH_SIM:        return "module_slot_switch_simulated";
    case PAYLOAD_BLOCKED_DRIVER_INSTALL: return "blocked_driver_install";
    case PAYLOAD_UNKNOWN:                return "unknown";
    }
    return "?";
}

/* ------------------------------------------------------------------ */
/* The built-in seed set — the four known packages, now as manifests.   */
/* ------------------------------------------------------------------ */

struct seed { struct pkg_manifest m; };
#define SEED_COUNT 4
static const struct pkg_manifest seeds[SEED_COUNT] = {
    { "PKG-001", "ghostattach_policy_update",
      "Update GhostAttach attach-policy 0.3 to 0.4",
      "GhostAttach", MOD_GHOSTATTACH, "0.3", "0.4",
      RISK_LOW, 1, 1, 0, SIG_PLACEHOLDER, TRUST_ANCHOR_ID, 1,
      PAYLOAD_SLOT_SWITCH_SIM, 0, 0, 0, "", "" },
    { "PKG-002", "recovery_analyzer_rules_update",
      "Update analyzer rule revision 0.4 to 0.5",
      "AI Recovery Core", MOD_AI_RULES, "0.4", "0.5",
      RISK_LOW, 1, 1, 0, SIG_PLACEHOLDER, TRUST_ANCHOR_ID, 1,
      PAYLOAD_SLOT_SWITCH_SIM, 0, 0, 0, "", "" },
    { /* the canary: wrong on every axis, must always be blocked */
      "PKG-003", "install_unknown_network_driver",
      "Install unverified net driver (FORBIDDEN)",
      "GhostAttach/network", MOD_GHOSTATTACH, "0.3", "?",
      RISK_HIGH, 0, 1, 0, SIG_UNSIGNED, 0 /* unknown anchor too! */, 1,
      PAYLOAD_BLOCKED_DRIVER_INSTALL, 0, 0, 0, "", "" },
    { "PKG-004", "safe_mode_policy_update",
      "Update safe-mode policy 0.5 to 0.6",
      "RecoveryEngine", MOD_RECOVERY_ENGINE, "0.5", "0.6",
      RISK_MEDIUM, 1, 1, 0, SIG_PLACEHOLDER, TRUST_ANCHOR_ID, 1,
      PAYLOAD_SLOT_SWITCH_SIM, 0, 0, 0, "", "" },
};

/* Checksum over every field that defines the manifest's meaning. */
static uint8_t manifest_checksum(const struct pkg_manifest *m)
{
    uint8_t sum = 0;
    const char *strs[6] = { m->package_id, m->name, m->description,
                            m->target_component, m->from_version,
                            m->to_version };
    for (int s = 0; s < 6; s++)
        for (const char *c = strs[s]; *c; c++)
            sum = (uint8_t)(sum + (uint8_t)*c);
    sum = (uint8_t)(sum + (uint8_t)m->risk_level + m->reversible +
                    m->requires_approval + (uint8_t)m->signature_status +
                    m->trust_anchor_id + m->manifest_version +
                    (uint8_t)m->payload_type + (uint8_t)m->target_module);
    return sum;
}

/* ------------------------------------------------------------------ */
/* Disk (de)serialization                                               */
/* ------------------------------------------------------------------ */

static void put_str(uint8_t *sec, int off, const char *s, int cap)
{
    int i = 0;
    for (; i < cap - 1 && s[i]; i++)
        sec[off + i] = (uint8_t)s[i];
    for (; i < cap; i++)
        sec[off + i] = 0;
}

static void get_str(const uint8_t *sec, int off, char *out, int cap)
{
    for (int i = 0; i < cap; i++)
        out[i] = (char)sec[off + i];
    out[cap - 1] = '\0';
}

static int write_manifest_sector(uint32_t slot, const struct pkg_manifest *m)
{
    uint8_t sec[ATA_SECTOR_SIZE];
    kmemset(sec, 0, sizeof(sec));
    sec[O_MAGIC] = MF_MAGIC0; sec[O_MAGIC+1] = MF_MAGIC1;
    sec[O_MAGIC+2] = MF_MAGIC2; sec[O_MAGIC+3] = MF_MAGIC3;
    sec[O_FMTVER] = MF_FMT_VERSION;
    put_str(sec, O_ID, m->package_id, PM_ID_MAX);
    put_str(sec, O_NAME, m->name, PM_NAME_MAX);
    put_str(sec, O_DESC, m->description, PM_DESC_MAX);
    put_str(sec, O_TARGET, m->target_component, PM_TARGET_MAX);
    put_str(sec, O_FROM, m->from_version, PM_VER_MAX);
    put_str(sec, O_TO, m->to_version, PM_VER_MAX);
    sec[O_RISK]    = (uint8_t)m->risk_level;
    sec[O_REV]     = m->reversible;
    sec[O_REQ]     = m->requires_approval;
    sec[O_SIG]     = (uint8_t)m->signature_status;
    sec[O_ANCHOR]  = m->trust_anchor_id;
    sec[O_MVER]    = m->manifest_version;
    sec[O_PAYLOAD] = (uint8_t)m->payload_type;
    sec[O_MODULE]  = (uint8_t)m->target_module;
    sec[O_CHECKSUM]= manifest_checksum(m);

    return ata_write_sector(LBA_MANIFESTS + slot, sec);
}

/* Returns 1 if the sector held a structurally valid manifest. */
static int read_manifest_sector(uint32_t slot, struct pkg_manifest *m)
{
    uint8_t sec[ATA_SECTOR_SIZE];
    if (!ata_read_sector(LBA_MANIFESTS + slot, sec))
        return 0;
    if (sec[O_MAGIC] != MF_MAGIC0 || sec[O_MAGIC+1] != MF_MAGIC1 ||
        sec[O_MAGIC+2] != MF_MAGIC2 || sec[O_MAGIC+3] != MF_MAGIC3 ||
        sec[O_FMTVER] != MF_FMT_VERSION)
        return 0;

    kmemset(m, 0, sizeof(*m));
    get_str(sec, O_ID, m->package_id, PM_ID_MAX);
    get_str(sec, O_NAME, m->name, PM_NAME_MAX);
    get_str(sec, O_DESC, m->description, PM_DESC_MAX);
    get_str(sec, O_TARGET, m->target_component, PM_TARGET_MAX);
    get_str(sec, O_FROM, m->from_version, PM_VER_MAX);
    get_str(sec, O_TO, m->to_version, PM_VER_MAX);
    m->risk_level        = (enum risk_level)sec[O_RISK];
    m->reversible        = sec[O_REV];
    m->requires_approval = sec[O_REQ];
    m->signature_status  = (enum signature_status)sec[O_SIG];
    m->trust_anchor_id   = sec[O_ANCHOR];
    m->manifest_version  = sec[O_MVER];
    m->payload_type      = (enum payload_type)sec[O_PAYLOAD];
    m->target_module     = (enum module_id)sec[O_MODULE];
    m->checksum          = sec[O_CHECKSUM];
    m->signature_present = sec[O_SIG_PRESENT];
    m->signature_algorithm = sec[O_SIG_ALG];
    get_str(sec, O_SIG_KEY_HASH, m->signer_key_hash, sizeof(m->signer_key_hash));
    get_str(sec, O_SIG_PAYLOAD_HASH, m->signed_payload_hash,
            sizeof(m->signed_payload_hash));

    /* The stored checksum must match a recomputation over the fields.
       checksum_ok feeds the verification chain (a mismatch = blocked). */
    m->checksum_ok = (m->checksum == manifest_checksum(m));
    return 1;
}

/* ------------------------------------------------------------------ */
/* Init / seed                                                          */
/* ------------------------------------------------------------------ */

static void load_builtin(void)
{
    count = SEED_COUNT;
    for (uint32_t i = 0; i < SEED_COUNT; i++) {
        manifests[i] = seeds[i];
        manifests[i].checksum    = manifest_checksum(&manifests[i]);
        manifests[i].checksum_ok = 1;
    }
    from_disk = 0;
}

void package_manifest_init(void)
{
    count = 0;
    from_disk = 0;

    if (ata_available()) {
        for (uint32_t slot = 0; slot < MANIFEST_MAX; slot++) {
            struct pkg_manifest m;
            if (read_manifest_sector(slot, &m) && count < MANIFEST_MAX)
                manifests[count++] = m;
        }
    }

    if (count > 0) {
        from_disk = 1;
        error_log(SEV_INFO, "MAN-000", "PackageManifest",
                  "Package manifests loaded from disk",
                  "verified against trust anchor and checksums");
    } else {
        load_builtin();
        error_log(SEV_INFO, "MAN-001", "PackageManifest",
                  "No manifests on disk - using built-in seed set",
                  "run seed-manifests confirm to persist them");
    }
}

int manifest_seed_to_disk(void)
{
    if (!ata_available())
        return 0;
    int written = 0;
    for (uint32_t i = 0; i < SEED_COUNT; i++) {
        struct pkg_manifest m = seeds[i];
        m.checksum = manifest_checksum(&m);
        if (write_manifest_sector(i, &m))
            written++;
    }
    /* Reload so RAM state == disk state, marked as disk-sourced. */
    package_manifest_init();
    error_log(SEV_INFO, "MAN-002", "PackageManifest",
              "Built-in manifests seeded to disk sectors",
              "manifests now persist across emulator restarts");
    return written;
}

uint32_t manifest_count(void) { return count; }

struct pkg_manifest *manifest_get(uint32_t index)
{
    return (index < count) ? &manifests[index] : 0;
}

struct pkg_manifest *manifest_find(const char *package_id)
{
    if (!package_id || package_id[0] == '\0')
        return 0;
    for (uint32_t i = 0; i < count; i++)
        if (kstricmp(manifests[i].package_id, package_id) == 0)
            return &manifests[i];
    return 0;
}

int manifest_source_is_disk(void) { return from_disk; }

const char *manifest_source_name(void)
{
    return from_disk ? "disk_imported" : "built_in_seed";
}

/* ------------------------------------------------------------------ */
/* The verification chain — every rule, in order, first failure wins.   */
/* ------------------------------------------------------------------ */

int manifest_verify(const struct pkg_manifest *m, const char **reason)
{
    if (!m->checksum_ok) {
        *reason = "checksum mismatch - manifest corrupt or tampered";
        return 0;
    }
    if (m->signature_status == SIG_UNSIGNED) {
        *reason = "unsigned - no signature at all";
        return 0;
    }
    if (m->signature_status == SIG_INVALID) {
        *reason = "signature invalid";
        return 0;
    }
    if (from_disk && m->signature_status != SIG_TRUSTED) {
        *reason = "imported manifests require trusted Ed25519 signature metadata";
        return 0;
    }
    if (from_disk && !manifest_signature_valid(m)) {
        *reason = "signature metadata missing or not host-verified";
        return 0;
    }
    if (m->trust_anchor_id != TRUST_ANCHOR_ID) {
        *reason = "unknown trust anchor - not vouched for by KMZ-ROOT-1";
        return 0;
    }
    if (m->payload_type == PAYLOAD_BLOCKED_DRIVER_INSTALL) {
        *reason = "payload type blocked_driver_install is never allowed";
        return 0;
    }
    if (m->payload_type == PAYLOAD_UNKNOWN) {
        *reason = "unknown payload type";
        return 0;
    }
    if (m->target_module >= MODULE_COUNT) {
        *reason = "unknown target component";
        return 0;
    }
    if (m->risk_level == RISK_HIGH) {
        *reason = "high-risk packages are blocked";
        return 0;
    }
    if (!m->reversible) {
        *reason = "irreversible packages are blocked by policy";
        return 0;
    }
    *reason = "passes verification - requires human approval";
    return 1;
}

int manifest_checksum_valid(const struct pkg_manifest *m)
{
    return m->checksum_ok ? 1 : 0;
}

int manifest_trust_valid(const struct pkg_manifest *m)
{
    if (m->signature_status == SIG_UNSIGNED || m->signature_status == SIG_INVALID)
        return 0;
    return m->trust_anchor_id == TRUST_ANCHOR_ID;
}

int manifest_signature_valid(const struct pkg_manifest *m)
{
    if (!from_disk)
        return m->signature_status == SIG_PLACEHOLDER ||
               m->signature_status == SIG_TRUSTED;
    return m->signature_status == SIG_TRUSTED &&
           m->signature_present == 2 &&
           m->signature_algorithm == SIG_ALG_ED25519;
}

const char *manifest_signature_mode(const struct pkg_manifest *m)
{
    if (!from_disk && m->signature_status == SIG_PLACEHOLDER)
        return "built_in_placeholder";
    if (manifest_signature_valid(m))
        return "simulated_kernel_check_host_ed25519";
    if (m->signature_present)
        return "present_but_untrusted";
    return "missing";
}

const char *manifest_policy_result(const struct pkg_manifest *m)
{
    const char *reason;
    return manifest_verify(m, &reason) ? "allowed" : "blocked";
}

const char *manifest_block_reason(const struct pkg_manifest *m)
{
    const char *reason;
    if (manifest_verify(m, &reason))
        return "none";
    return reason;
}
