/*
 * errors.h — the kernel's structured error log.
 *
 * THIS IS THE MOST IMPORTANT "AI-NATIVE" DESIGN DECISION IN KAMIKAZEOS.
 * Minimal kernels often print loose text. KamikazeOS records *structured
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

/*
 * v2.6.1 health classification. Every event keeps its severity and is
 * logged and exported exactly as before; the class only decides whether
 * it costs PRODUCTION health. Unclassified call sites default to
 * HEALTH_PRODUCTION, so a new fault can never be excluded by accident.
 */
enum health_class {
    HEALTH_PRODUCTION = 0,       /* a real fault: counts toward health    */
    HEALTH_OPERATOR_INPUT,       /* typo, invalid argument, or an operator */
                                 /* request refused by policy             */
    HEALTH_EXPECTED_CANARY,      /* compiled-in canary blocked as designed */
    HEALTH_INTENTIONAL_TEST,     /* operator-triggered test fault/panic    */
    HEALTH_DEVICE_STATE,         /* already scored via device status       */
    HEALTH_CLASS_COUNT
};

struct kerror {
    uint32_t           seq;        /* global event number, never reused */
    enum err_severity  severity;
    enum health_class  health;     /* v2.6.1: production or excluded     */
    char               code[ERR_CODE_MAX];
    char               component[ERR_COMP_MAX];
    char               message[ERR_MSG_MAX];
    char               hint[ERR_HINT_MAX];
};

void errors_init(void);

/* Record an event. Also streams it to the serial port immediately. */
void error_log(enum err_severity severity, const char *code,
               const char *component, const char *message, const char *hint);

/* Same, with an explicit health class (error_log == HEALTH_PRODUCTION).
   Non-production entries carry `;health=<class>` on serial so the host
   can see exactly what was excluded from the score. */
void error_log_class(enum err_severity severity, enum health_class health,
                     const char *code, const char *component,
                     const char *message, const char *hint);

/* Events of this class and severity logged THIS BOOT. Cumulative counters:
   unlike the 64-entry ring they never drop old events, so the health
   score cannot improve just because the ring wrapped. */
uint32_t error_health_count(enum health_class health, enum err_severity sev);
const char *health_class_name(enum health_class health);

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
