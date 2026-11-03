#ifndef KMZ_APP_H
#define KMZ_APP_H

#include <stdint.h>

#define APP_MAX 8
#define APP_NAME_MAX 24
#define APP_DESC_MAX 64
#define APP_VERSION_MAX 8
#define APP_LOG_MAX 16

enum app_type {
    APP_BUILTIN = 0,
    APP_DIAGNOSTIC,
    APP_TINYFS_TOOL,
    APP_RECOVERY_TOOL,
    APP_DEMO,
};

enum app_status {
    APP_AVAILABLE = 0,
    APP_RUNNING,
    APP_COMPLETED,
    APP_FAILED,
    APP_BLOCKED,
};

typedef int (*app_entry_fn)(void);

struct app {
    uint32_t app_id;
    const char *name;
    const char *description;
    const char *version;
    enum app_type type;
    enum app_status status;
    app_entry_fn entry;
    uint32_t run_count;
    const char *last_result;
};

struct app_log_entry {
    uint32_t seq;
    char name[APP_NAME_MAX];
    const char *result;
    const char *status;
    const char *message;
};

void apps_init(void);
uint32_t app_count(void);
const struct app *app_get(uint32_t index);
const struct app *app_find(const char *name);
int app_run(const char *name);
uint32_t app_log_list(const struct app_log_entry **out, uint32_t capacity);
const char *app_type_name(enum app_type type);
const char *app_status_name(enum app_status status);
const char *app_last_name(void);
const char *app_last_result(void);
uint32_t app_blocked_attempts(void);
void app_export_report(void);

#endif
