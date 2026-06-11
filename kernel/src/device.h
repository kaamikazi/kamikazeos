/*
 * device.h — the device model for GhostAttach.
 *
 * A "device" here is one piece of hardware (real or, in v0.3, simulated)
 * that the OS knows about: what it is, which bus it sits on, which driver
 * it NEEDS, which safe fallback driver could stand in, and what state the
 * GhostAttach boot scan left it in.
 *
 * v0.3 reality check: the device registry is a STATIC table compiled into
 * the kernel (see ghostattach.c). Real PCI bus scanning replaces the static
 * entries in a later phase — the structs and states are designed so that
 * swap requires no changes to the policy engine or the shell.
 */
#ifndef KMZ_DEVICE_H
#define KMZ_DEVICE_H

#include <stdint.h>

/* What kind of hardware is this? (DEV_SERIAL is an addition to the base
   list because KamikazeOS really has a COM1 UART doing AI log export —
   pretending it doesn't exist would make the registry lie.) */
enum device_type {
    DEV_DISPLAY = 0,
    DEV_KEYBOARD,
    DEV_STORAGE,
    DEV_NETWORK,
    DEV_AUDIO,
    DEV_SERIAL,
    DEV_UNKNOWN,
};

/* Which bus the device hangs off. */
enum bus_type {
    BUS_BUILTIN = 0,   /* part of the platform itself (VGA, UART)    */
    BUS_PS2,
    BUS_PCI,
    BUS_USB,
    BUS_VIRTUAL,       /* exists only inside the emulator/simulation */
};

/* What state the GhostAttach scan left the device in. */
enum device_status {
    DEVST_UNKNOWN = 0,        /* not scanned yet                          */
    DEVST_ATTACHED,           /* exact verified driver loaded             */
    DEVST_FALLBACK,           /* verified generic driver standing in      */
    DEVST_MISSING_DRIVER,     /* no verified driver exists; device off    */
    DEVST_FAILED,             /* a driver tried to attach and failed      */
    DEVST_ISOLATED,           /* quarantined after failure (rule 6)       */
};

struct device {
    uint8_t            device_id;
    const char        *name;             /* human name: "PS/2 Keyboard"   */
    const char        *vendor;
    enum device_type   device_type;
    enum bus_type      bus_type;
    const char        *required_driver;  /* exact driver name, or 0       */
    const char        *fallback_driver;  /* safe generic driver name, or 0*/
    enum device_status status;           /* written by the boot scan      */
    const char        *attached_driver;  /* what actually loaded, or "none" */
};

/* Human-readable names for the enums (used by the shell and the logs). */
static inline const char *device_type_name(enum device_type t)
{
    switch (t) {
    case DEV_DISPLAY:  return "display";
    case DEV_KEYBOARD: return "keyboard";
    case DEV_STORAGE:  return "storage";
    case DEV_NETWORK:  return "network";
    case DEV_AUDIO:    return "audio";
    case DEV_SERIAL:   return "serial";
    case DEV_UNKNOWN:  return "unknown";
    }
    return "?";
}

static inline const char *bus_type_name(enum bus_type b)
{
    switch (b) {
    case BUS_BUILTIN: return "builtin";
    case BUS_PS2:     return "ps2";
    case BUS_PCI:     return "pci";
    case BUS_USB:     return "usb";
    case BUS_VIRTUAL: return "virtual";
    }
    return "?";
}

static inline const char *device_status_name(enum device_status s)
{
    switch (s) {
    case DEVST_UNKNOWN:        return "unknown";
    case DEVST_ATTACHED:       return "attached";
    case DEVST_FALLBACK:       return "fallback";
    case DEVST_MISSING_DRIVER: return "missing_driver";
    case DEVST_FAILED:         return "failed";
    case DEVST_ISOLATED:       return "isolated";
    }
    return "?";
}

#endif /* KMZ_DEVICE_H */
