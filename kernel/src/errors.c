/* errors.c — structured ring-buffer error log. See errors.h for intent. */
#include "errors.h"
#include "serial.h"
#include "string.h"

static struct kerror ring[ERR_CAPACITY];
static uint32_t next_seq;     /* total events ever logged                */
static uint32_t stored;       /* how many entries currently in the ring */
static uint32_t class_counts[HEALTH_CLASS_COUNT][4];   /* per boot, never wrap */

const char *severity_name(enum err_severity sev)
{
    switch (sev) {
    case SEV_INFO:    return "info";
    case SEV_WARNING: return "warning";
    case SEV_ERROR:   return "error";
    case SEV_PANIC:   return "panic";
    }
    return "unknown";
}

void errors_init(void)
{
    next_seq = 0;
    stored   = 0;
    kmemset(ring, 0, sizeof(ring));
    kmemset(class_counts, 0, sizeof(class_counts));
}

/*
 * Emit one entry to the serial port in the AI-readable key=value format —
 * the exact same format the `logs-ai` shell command prints on screen:
 *   code=DRV-101;severity=warning;component=GhostAttach;message=...;hint=...
 * One line per event, fields separated by ';', no quoting (messages are
 * kernel-authored constants, so they never contain ';' or '=').
 */
static void emit_serial(const struct kerror *e)
{
    serial_print("code=");      serial_print_field(e->code);
    serial_print(";severity="); serial_print(severity_name(e->severity));
    serial_print(";component=");serial_print_field(e->component);
    serial_print(";message=");  serial_print_field(e->message);
    serial_print(";hint=");     serial_print_field(e->hint);
    if (e->health != HEALTH_PRODUCTION) {
        serial_print(";health=");
        serial_print(health_class_name(e->health));
    }
    serial_print("\n");
}

const char *health_class_name(enum health_class health)
{
    switch (health) {
    case HEALTH_PRODUCTION:       return "production";
    case HEALTH_OPERATOR_INPUT:   return "excluded_operator_input";
    case HEALTH_EXPECTED_CANARY:  return "excluded_expected_canary";
    case HEALTH_INTENTIONAL_TEST: return "excluded_intentional_test";
    case HEALTH_DEVICE_STATE:     return "scored_via_device_state";
    case HEALTH_CLASS_COUNT:      break;
    }
    return "production";
}

void error_log(enum err_severity severity, const char *code,
               const char *component, const char *message, const char *hint)
{
    error_log_class(severity, HEALTH_PRODUCTION, code, component, message, hint);
}

uint32_t error_health_count(enum health_class health, enum err_severity sev)
{
    if ((uint32_t)health >= HEALTH_CLASS_COUNT || (uint32_t)sev > SEV_PANIC)
        return 0;
    return class_counts[health][sev];
}

void error_log_class(enum err_severity severity, enum health_class health,
                     const char *code, const char *component,
                     const char *message, const char *hint)
{
    if ((uint32_t)health >= HEALTH_CLASS_COUNT)
        health = HEALTH_PRODUCTION;
    if ((uint32_t)severity <= SEV_PANIC &&
        class_counts[health][severity] != 0xFFFFFFFFu)
        class_counts[health][severity]++;

    /* The slot to write: sequence number modulo capacity = ring position. */
    struct kerror *e = &ring[next_seq % ERR_CAPACITY];

    e->seq      = next_seq;
    e->severity = severity;
    e->health   = health;
    kstrcpy(e->code,      code,      ERR_CODE_MAX);
    kstrcpy(e->component, component, ERR_COMP_MAX);
    kstrcpy(e->message,   message,   ERR_MSG_MAX);
    kstrcpy(e->hint,      hint,      ERR_HINT_MAX);

    next_seq++;
    if (stored < ERR_CAPACITY)
        stored++;

    /*
     * Stream to the host IMMEDIATELY. If the kernel dies one instruction
     * after this call, the host still has the evidence. Diagnostics that
     * only live in RAM die with the machine.
     */
    emit_serial(e);
}

uint32_t error_count(void)
{
    return stored;
}

uint32_t error_count_severity(enum err_severity sev)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < stored; i++) {
        uint32_t first = (stored == ERR_CAPACITY) ? (next_seq % ERR_CAPACITY) : 0;
        if (ring[(first + i) % ERR_CAPACITY].severity == sev)
            n++;
    }
    return n;
}

int error_get(uint32_t index, struct kerror *out)
{
    if (index >= stored)
        return 0;

    /*
     * Oldest entry: if the ring has wrapped, the oldest is the one that
     * will be overwritten next (next_seq % CAPACITY); otherwise it's 0.
     */
    uint32_t first = (stored == ERR_CAPACITY) ? (next_seq % ERR_CAPACITY) : 0;
    *out = ring[(first + index) % ERR_CAPACITY];
    return 1;
}
