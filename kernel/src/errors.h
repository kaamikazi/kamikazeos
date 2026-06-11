/*
 * errors.h — the kernel's structured error log.
 *
 * THIS IS THE MOST IMPORTANT "AI-NATIVE" DESIGN DECISION IN KAMIKAZEOS.
 * Normal toy kernels print loose text. KamikazeOS records *structured
 * events*: every entry has a stable error CODE, a severity, the COMPONENT
 * that raised it, a human-readable message, and — new in v0.2 — a
 * RECOVERY HINT describing what could be done about it.
 *
 * Why a hint field? The future AI Recovery Core (Phase 4) ranks fixes by
 * safety. Teaching every component to state its own best-known remedy at
 * the moment of failure gives the analyzer ground truth to start from,
 * instead of guessing from the message text.
 *
 * Every entry is ALSO streamed out the serial port the moment it is logged
 * (see serial.h), in the same machine-parseable key=value format that the
 * `logs-ai` shell command prints. Run QEMU with
 *     -serial file:build/serial.log
 * and the host gets a live, AI-readable feed of everything that happened —
 * even if the kernel dies right after logging.
 *
 * Entries live in a fixed-size ring buffer: when it fills, the oldest entry
 * is overwritten (the serial copy is the permanent record). No heap needed.
 */
#ifndef KMZ_ERRORS_H
#define KMZ_ERRORS_H

#include <stdint.h>

enum err_severity {
    SEV_INFO    = 0,   /* normal lifecycle events                 */
    SEV_WARNING = 1,   /* something odd but survivable            */
    SEV_ERROR   = 2,   /* a component failed                      */
    SEV_PANIC   = 3,   /* the kernel itself is going down         */
};

#define ERR_CODE_MAX 16    /* e.g. "DRV-101"          */
#define ERR_COMP_MAX 20    /* e.g. "GhostAttach"      */
#define ERR_MSG_MAX  80    /* human-readable message (fits composed
                              "verb ACT-xxx long_action_name" strings) */
#define ERR_HINT_MAX 64    /* best-known remedy (sized so real hints fit) */
#define ERR_CAPACITY 64    /* ring buffer size        */

struct kerror {
    uint32_t           seq;        /* global event number, never reused */
    enum err_severity  severity;
    char               code[ERR_CODE_MAX];
    char               component[ERR_COMP_MAX];
    char               message[ERR_MSG_MAX];
    char               hint[ERR_HINT_MAX];
};

void errors_init(void);

/* Record an event. Also streams it to the serial port immediately. */
void error_log(enum err_severity severity, const char *code,
               const char *component, const char *message, const char *hint);

/*
 * Read entries back, oldest first. Pass an index 0..error_count()-1.
 * Returns 0 if the index is out of range.
 */
int      error_get(uint32_t index, struct kerror *out);
uint32_t error_count(void);

/* How many logged entries have exactly this severity (for health math). */
uint32_t error_count_severity(enum err_severity sev);

/* "info" / "warning" / "error" / "panic" — shared by shell and serial. */
const char *severity_name(enum err_severity sev);

#endif /* KMZ_ERRORS_H */
