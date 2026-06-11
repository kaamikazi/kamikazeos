/*
 * driver.h — the driver model for GhostAttach.
 *
 * A "driver" is a piece of code that can operate one device type. The
 * registry entry records the safety facts GhostAttach's policy engine
 * decides with: is it VERIFIED (signed/trusted), and what is its RISK
 * level if it misbehaves?
 *
 * THE CENTRAL SAFETY RULE OF GHOSTATTACH lives on these two fields:
 * an unverified driver is NEVER loaded, no matter how convenient —
 * not by the boot scan, not by the shell, and later not by the AI either.
 *
 * v0.3 reality check: drivers in this phase are registry entries plus the
 * built-in code that already exists (VGA text, PS/2 keyboard, COM1 UART).
 * There is no dynamic code loading yet — "attach" means "policy-approve
 * and bind the built-in/simulated driver to the device".
 */
#ifndef KMZ_DRIVER_H
#define KMZ_DRIVER_H

#include <stdint.h>
#include "device.h"        /* enum device_type */

enum driver_status {
    DRVST_AVAILABLE = 0,        /* in the registry, not bound to anything */
    DRVST_LOADED,               /* bound to at least one device           */
    DRVST_MISSING,              /* referenced by a device but not present */
    DRVST_FAILED,               /* tried to attach and failed             */
    DRVST_BLOCKED_UNVERIFIED,   /* refused by policy: not verified        */
};

enum risk_level {
    RISK_LOW = 0,      /* output-only or well-understood (VGA text)      */
    RISK_MEDIUM,       /* touches input or shared state                  */
    RISK_HIGH,         /* can corrupt data or hang the bus (storage/DMA) */
};

struct driver {
    uint8_t             driver_id;
    const char         *name;                 /* "vga_text_driver"       */
    enum device_type    supports_device_type; /* one type per driver for now */
    const char         *version;
    int                 verified;             /* 1 = signed/trusted      */
    enum risk_level     risk_level;
    enum driver_status  status;               /* written by the boot scan */

    /*
     * v0.3 SIMULATION FIELD — honest about what it is. Real driver attach
     * means probing hardware, which we can only truly do for the PS/2 and
     * COM1 devices today. For everything else this flag decides whether
     * the simulated attach "succeeds", so we can exercise (and test!) the
     * failure->isolation policy path. Removed when real probing arrives.
     */
    int                 sim_attach_works;
};

static inline const char *driver_status_name(enum driver_status s)
{
    switch (s) {
    case DRVST_AVAILABLE:          return "available";
    case DRVST_LOADED:             return "loaded";
    case DRVST_MISSING:            return "missing";
    case DRVST_FAILED:             return "failed";
    case DRVST_BLOCKED_UNVERIFIED: return "blocked_unverified";
    }
    return "?";
}

static inline const char *risk_level_name(enum risk_level r)
{
    switch (r) {
    case RISK_LOW:    return "low";
    case RISK_MEDIUM: return "medium";
    case RISK_HIGH:   return "high";
    }
    return "?";
}

#endif /* KMZ_DRIVER_H */
