/* module_versions.c — the component version table. See module_versions.h. */
#include "module_versions.h"
#include "string.h"

static struct module_version modules[MODULE_COUNT] = {
    /* name                       version  base   updated */
    { "Kernel",                   "1.7",   "1.7", 0 },
    { "GhostAttach",              "0.3",   "0.3", 0 },
    { "RecoveryEngine",           "0.5",   "0.5", 0 },
    { "RecoveryMemory",           "0.6",   "0.6", 0 },
    { "RecoveryStore",            "0.7",   "0.7", 0 },
    { "AI Recovery Core Rules",   "0.4",   "0.4", 0 },
    { "Recovery Center Schema",   "0.8",   "0.8", 0 },
};

void module_versions_init(void)
{
    /* Versions reset to compiled-in base values, then package state and
       A/B slot metadata restore any verified simulated updates. */
    for (int i = 0; i < MODULE_COUNT; i++) {
        modules[i].version = modules[i].base_version;
        modules[i].updated = 0;
    }
}

uint32_t module_count(void)
{
    return MODULE_COUNT;
}

const struct module_version *module_get(uint32_t index)
{
    return (index < MODULE_COUNT) ? &modules[index] : 0;
}

const char *module_version_of(enum module_id id)
{
    return modules[id].version;
}

void module_set_version(enum module_id id, const char *version)
{
    modules[id].version = version;
    modules[id].updated = (kstrcmp(version, modules[id].base_version) != 0);
}
