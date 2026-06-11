/*
 * module_versions.h — version tracking for KamikazeOS's own components.
 *
 * An OS that upgrades itself must first KNOW WHAT IT IS RUNNING. This
 * module is the single source of truth for component versions. Recovery
 * packages (recovery_package.c) are the ONLY thing allowed to change a
 * version — and in v0.9 a "version change" is exactly that: metadata.
 * No code changes. The honest foundation for real A/B updates later.
 */
#ifndef KMZ_MODULE_VERSIONS_H
#define KMZ_MODULE_VERSIONS_H

#include <stdint.h>

enum module_id {
    MOD_KERNEL = 0,
    MOD_GHOSTATTACH,
    MOD_RECOVERY_ENGINE,
    MOD_RECOVERY_MEMORY,
    MOD_RECOVERY_STORE,
    MOD_AI_RULES,          /* analyzer rule revision the kernel expects */
    MOD_RC_SCHEMA,         /* Recovery Center report schema             */
    MODULE_COUNT
};

struct module_version {
    const char *name;          /* "GhostAttach"                          */
    const char *version;       /* CURRENT version (may be updated)       */
    const char *base_version;  /* version compiled into this kernel      */
    int         updated;       /* 1 = a package changed it this session  */
};

void module_versions_init(void);
uint32_t module_count(void);
const struct module_version *module_get(uint32_t index);
const char *module_version_of(enum module_id id);

/* Called ONLY by recovery_package.c during apply/rollback. */
void module_set_version(enum module_id id, const char *version);

#endif /* KMZ_MODULE_VERSIONS_H */
