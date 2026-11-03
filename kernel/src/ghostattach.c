/*
 * ghostattach.c — the GhostAttach policy engine + v0.3 static registries.
 *
 * Read ghostattach.h first for the six policy rules. This file implements
 * them in try_attach_driver() and runs them over every device in
 * ghostattach_scan(). Everything is fixed-size static data: no heap, no
 * dynamic loading — exactly what a kernel this young should be doing.
 */
#include "ghostattach.h"
#include "errors.h"
#include "keyboard.h"
#include "serial.h"
#include "string.h"

/* ------------------------------------------------------------------ */
/* The driver registry (v0.3: compiled in; later: signed manifest)      */
/* ------------------------------------------------------------------ */
/*
 * Entries chosen to exercise every policy path on purpose:
 *  - verified low-risk drivers that work        -> attached / fallback
 *  - an UNVERIFIED high-risk fallback           -> blocked (rule 3)
 *  - a verified driver whose attach fails       -> isolation (rule 5)
 */
#define DRIVER_COUNT 6
static struct driver drivers[DRIVER_COUNT] = {
    /* id name                   supports      ver   verified risk        status           sim_ok */
    { 1, "vga_text_driver",      DEV_DISPLAY,  "0.3", 1, RISK_LOW,    DRVST_AVAILABLE, 1 },
    { 2, "ps2_keyboard_driver",  DEV_KEYBOARD, "0.3", 1, RISK_LOW,    DRVST_AVAILABLE, 1 },
    { 3, "com1_serial_driver",   DEV_SERIAL,   "0.3", 1, RISK_LOW,    DRVST_AVAILABLE, 1 },
    /* storage fallback exists but is NOT verified: policy must block it. */
    { 4, "generic_storage",      DEV_STORAGE,  "0.1", 0, RISK_HIGH,   DRVST_AVAILABLE, 1 },
    /* verified audio fallback: legal stand-in when the exact driver is missing. */
    { 5, "legacy_audio_fallback",DEV_AUDIO,    "0.2", 1, RISK_MEDIUM, DRVST_AVAILABLE, 1 },
    /* verified driver that FAILS its (simulated) self-test: tests isolation. */
    { 6, "selftest_driver",      DEV_UNKNOWN,  "0.1", 1, RISK_LOW,    DRVST_AVAILABLE, 0 },
};

/* ------------------------------------------------------------------ */
/* The device registry (v0.3: static; later: filled by a real PCI scan) */
/* ------------------------------------------------------------------ */
#define DEVICE_COUNT 7
static struct device devices[DEVICE_COUNT] = {
    /* id name                  vendor       type          bus          required_driver        fallback_driver         status         attached */
    { 1, "VGA Text Display",    "QEMU",      DEV_DISPLAY,  BUS_BUILTIN, "vga_text_driver",      0,                      DEVST_UNKNOWN, "none" },
    { 2, "PS/2 Keyboard",       "QEMU",      DEV_KEYBOARD, BUS_PS2,     "ps2_keyboard_driver",  0,                      DEVST_UNKNOWN, "none" },
    { 3, "COM1 Serial Port",    "QEMU",      DEV_SERIAL,   BUS_BUILTIN, "com1_serial_driver",   0,                      DEVST_UNKNOWN, "none" },
    /* exact driver absent; only fallback is the UNVERIFIED generic_storage. */
    { 4, "Storage Controller",  "simulated", DEV_STORAGE,  BUS_PCI,     "ahci_storage_driver",  "generic_storage",      DEVST_UNKNOWN, "none" },
    /* exact driver absent; no fallback at all. */
    { 5, "Network Adapter",     "simulated", DEV_NETWORK,  BUS_PCI,     "e1000_net_driver",     0,                      DEVST_UNKNOWN, "none" },
    /* exact driver absent; fallback exists AND is verified -> legal stand-in. */
    { 6, "Audio Device",        "simulated", DEV_AUDIO,    BUS_PCI,     "hda_audio_driver",     "legacy_audio_fallback",DEVST_UNKNOWN, "none" },
    /* exact driver exists+verified but fails its self-test -> isolation path. */
    { 7, "Self-Test Device",    "kamikaze",  DEV_UNKNOWN,  BUS_VIRTUAL, "selftest_driver",      0,                      DEVST_UNKNOWN, "none" },
};

static struct ghostattach_stats stats;

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */

/* Look a driver up by name. Returns 0 if it isn't in the registry —
   which is itself meaningful: the device names a driver we don't have. */
static struct driver *find_driver(const char *name)
{
    if (!name)
        return 0;
    for (int i = 0; i < DRIVER_COUNT; i++)
        if (kstrcmp(drivers[i].name, name) == 0)
            return &drivers[i];
    return 0;
}

static int is_expected_self_test(const struct device *dev,
                                 const struct driver *drv)
{
    return dev && drv && dev->device_id == 7 && drv->driver_id == 6 &&
           dev->bus_type == BUS_VIRTUAL;
}

static int is_expected_self_test_device(const struct device *dev)
{
    return dev && dev->device_id == 7 && dev->bus_type == BUS_VIRTUAL;
}

/*
 * Compose "DeviceName + action" log messages without a printf. Messages
 * must never contain ';' or '=' (they would corrupt the AI log format).
 */
static void log_ga_class(enum err_severity sev, enum health_class cls,
                         const char *code, const char *devname,
                         const char *action, const char *hint)
{
    char msg[ERR_MSG_MAX];
    /* msg = devname + " " + action, truncated safely. */
    kstrcpy(msg, devname, ERR_MSG_MAX);
    size_t len = kstrlen(msg);
    if (len < ERR_MSG_MAX - 1) {
        msg[len++] = ' ';
        msg[len] = '\0';
        kstrcpy(msg + len, action, ERR_MSG_MAX - len);
    }
    error_log_class(sev, cls, code, "GhostAttach", msg, hint);
}

static void log_ga(enum err_severity sev, const char *code,
                   const char *devname, const char *action, const char *hint)
{
    log_ga_class(sev, HEALTH_PRODUCTION, code, devname, action, hint);
}

/*
 * "Probe" the hardware for a device. v0.3 honesty: for the PS/2 keyboard
 * and COM1 UART we have REAL probe results from boot; everything else is
 * simulated through the driver's sim_attach_works flag.
 */
static int probe_succeeds(const struct device *dev, const struct driver *drv)
{
    switch (dev->device_type) {
    case DEV_KEYBOARD: return keyboard_present();
    case DEV_SERIAL:   return serial_present();
    default:           return drv->sim_attach_works;
    }
}

/* ------------------------------------------------------------------ */
/* The policy engine                                                    */
/* ------------------------------------------------------------------ */

/*
 * Try to bind one driver to one device, applying the safety rules.
 * `is_fallback` only changes the resulting status/log code, not the rules.
 * Returns 1 if the device ended up usable (attached or fallback).
 */
static int try_attach_driver(struct device *dev, struct driver *drv,
                             int is_fallback)
{
    /* RULE 3: unverified drivers are blocked. Always. The check comes
       BEFORE anything touches the device, and high-risk unverified is the
       worst case — log it as a security event, not a driver event. */
    if (!drv->verified) {
        drv->status = DRVST_BLOCKED_UNVERIFIED;
        /* Classification is deferred until scan_device knows the final state. */
        return 0;
    }

    /* Probe: does the hardware actually answer? (real or simulated) */
    if (!probe_succeeds(dev, drv)) {
        /* RULE 5: a failing driver gets the device QUARANTINED, so a bad
           attach can't be retried into a boot loop. */
        drv->status = DRVST_FAILED;
        dev->status = DEVST_ISOLATED;
        if (is_expected_self_test(dev, drv)) {
            log_ga(SEV_INFO, "GA-TEST-201", dev->name,
                   "expected canary attach failure - test device isolated",
                   "self-test evidence only - no production fault");
        } else {
            log_ga(SEV_ERROR, "GA-DRV-201", dev->name,
                   "driver failed to attach - device isolated",
                   "device quarantined until a working driver exists");
        }
        return 0;
    }

    /* Success: bind it. */
    drv->status = DRVST_LOADED;
    dev->attached_driver = drv->name;
    if (is_fallback) {
        dev->status = DEVST_FALLBACK;
        log_ga(SEV_INFO, "GA-DRV-002", dev->name,
               "attached via verified fallback driver",
               "reduced functionality until exact driver exists");
    } else {
        dev->status = DEVST_ATTACHED;
        log_ga(SEV_INFO, "GA-DRV-001", dev->name,
               "attached to exact verified driver", "no action needed");
    }
    return 1;
}

static void log_blocked_candidates(const struct device *dev,
                                   const struct driver *exact,
                                   const struct driver *fallback)
{
    enum health_class cls = (dev->status == DEVST_MISSING_DRIVER ||
                             dev->status == DEVST_ISOLATED ||
                             dev->status == DEVST_FAILED)
                            ? HEALTH_DEVICE_STATE : HEALTH_PRODUCTION;
    if (exact && !exact->verified)
        log_ga_class(SEV_WARNING, cls, "GA-SEC-001", dev->name,
                     "driver blocked - not verified", "only load signed verified drivers");
    if (fallback && !fallback->verified)
        log_ga_class(SEV_WARNING, cls, "GA-SEC-001", dev->name,
                     "fallback driver blocked - not verified", "only load signed verified drivers");
}

/* Decide the fate of one device: exact -> fallback -> missing_driver. */
static void scan_device(struct device *dev)
{
    /* RULE 1: prefer the exact driver. */
    struct driver *exact = find_driver(dev->required_driver);
    if (exact) {
        if (try_attach_driver(dev, exact, 0))
            return;
        /* Exact driver exists but was blocked or failed. If it FAILED the
           device is now isolated — leave it quarantined (rule 5), don't
           paper over a hardware/driver fault with a fallback. */
        if (dev->status == DEVST_ISOLATED)
            return;
    }

    /* RULE 2: exact driver missing (or blocked) -> try the fallback. */
    struct driver *fb = find_driver(dev->fallback_driver);
    if (fb) {
        if (try_attach_driver(dev, fb, 1)) {
            log_blocked_candidates(dev, exact, fb);
            return;
        }
        if (dev->status == DEVST_ISOLATED) {
            log_blocked_candidates(dev, exact, fb);
            return;
        }
    }

    /* RULE 4: nothing verified could drive it. Device stays off, and the
       log explains both what happened and the safest next step. */
    dev->status = DEVST_MISSING_DRIVER;
    log_blocked_candidates(dev, exact, fb);
    /* Scored once, through missing_penalty (-10 per device). */
    log_ga_class(SEV_WARNING, HEALTH_DEVICE_STATE, "GA-DRV-101", dev->name,
           "missing verified driver",
           "keep device disabled until verified driver exists");
}

/* Essential = the machine is unusable/undiagnosable without it. */
static int is_essential(const struct device *dev)
{
    return dev->device_type == DEV_DISPLAY ||
           dev->device_type == DEV_KEYBOARD ||
           dev->device_type == DEV_SERIAL;
}

void ghostattach_scan(int safe_mode)
{
    error_log(SEV_INFO, "GA-BOOT-000", "GhostAttach",
              safe_mode ? "GhostAttach boot scan started in SAFE MODE"
                        : "GhostAttach boot scan started",
              "no action needed");

    kmemset(&stats, 0, sizeof(stats));
    stats.total = DEVICE_COUNT;

    for (int i = 0; i < DEVICE_COUNT; i++) {
        if (safe_mode && !is_essential(&devices[i])) {
            /* Safe mode: don't even TRY non-essential drivers. */
            devices[i].status = DEVST_ISOLATED;
            devices[i].attached_driver = "none";
            stats.isolated++;   /* skipped devices still count in stats */
            log_ga(SEV_INFO, "GA-SAFE-001", devices[i].name,
                   "skipped in safe mode - non-essential device",
                   "device returns on the next normal boot");
            continue;
        }
        scan_device(&devices[i]);

        switch (devices[i].status) {
        case DEVST_ATTACHED:       stats.attached++; break;
        case DEVST_FALLBACK:       stats.fallback++; break;
        case DEVST_MISSING_DRIVER: stats.missing++;  break;
        case DEVST_ISOLATED:
        case DEVST_FAILED:
            stats.isolated++;
            if (!safe_mode && is_expected_self_test_device(&devices[i]))
                stats.expected_test_failures++;
            break;
        default:                                     break;
        }
    }

    error_log(SEV_INFO, "GA-BOOT-001", "GhostAttach",
              "GhostAttach boot scan complete", "run devices for details");
}

/* ------------------------------------------------------------------ */
/* Read-only accessors for the shell                                    */
/* ------------------------------------------------------------------ */

uint32_t ghostattach_device_count(void) { return DEVICE_COUNT; }
uint32_t ghostattach_driver_count(void) { return DRIVER_COUNT; }

const struct device *ghostattach_device(uint32_t index)
{
    return (index < DEVICE_COUNT) ? &devices[index] : 0;
}

const struct driver *ghostattach_driver(uint32_t index)
{
    return (index < DRIVER_COUNT) ? &drivers[index] : 0;
}

void ghostattach_get_stats(struct ghostattach_stats *out)
{
    *out = stats;
}

/* ------------------------------------------------------------------ */
/* RecoveryEngine mutators (see ghostattach.h for the contract)         */
/* ------------------------------------------------------------------ */

static struct device *first_of_type(enum device_type type)
{
    for (int i = 0; i < DEVICE_COUNT; i++)
        if (devices[i].device_type == type)
            return &devices[i];
    return 0;
}

enum device_status ghostattach_get_status_by_type(enum device_type type)
{
    struct device *d = first_of_type(type);
    return d ? d->status : DEVST_UNKNOWN;
}

void ghostattach_set_status_by_type(enum device_type type,
                                    enum device_status status)
{
    struct device *d = first_of_type(type);
    if (!d)
        return;

    /* Keep the scan statistics honest after the change. */
    switch (d->status) {
    case DEVST_ATTACHED:       stats.attached--; break;
    case DEVST_FALLBACK:       stats.fallback--; break;
    case DEVST_MISSING_DRIVER: stats.missing--;  break;
    case DEVST_ISOLATED:
    case DEVST_FAILED:
        stats.isolated--;
        if (is_expected_self_test_device(d) && stats.expected_test_failures)
            stats.expected_test_failures--;
        break;
    default:                                     break;
    }
    d->status = status;
    switch (status) {
    case DEVST_ATTACHED:       stats.attached++; break;
    case DEVST_FALLBACK:       stats.fallback++; break;
    case DEVST_MISSING_DRIVER: stats.missing++;  break;
    case DEVST_ISOLATED:
    case DEVST_FAILED:
        stats.isolated++;
        if (is_expected_self_test_device(d))
            stats.expected_test_failures++;
        break;
    default:                                     break;
    }
}
