#include "app.h"
#include "app_policy.h"
#include "boot_snapshot.h"
#include "errors.h"
#include "module_versions.h"
#include "recovery_memory.h"
#include "recovery_store.h"
#include "serial.h"
#include "string.h"
#include "task.h"
#include "timer.h"
#include "tinyfs.h"
#include "vga.h"

static int app_hello(void);
static int app_sysinfo(void);
static int app_tinyfs_demo(void);
static int app_recovery_summary(void);
static int app_task_demo(void);
static int app_health_report(void);

static struct app apps[APP_MAX] = {
    { 1, "hello", "Print a friendly app-model greeting", "1.0",
      APP_DEMO, APP_AVAILABLE, app_hello, 0, "never_run" },
    { 2, "sysinfo", "Show kernel timing and module version basics", "1.0",
      APP_DIAGNOSTIC, APP_AVAILABLE, app_sysinfo, 0, "never_run" },
    { 3, "tinyfs-demo", "Create write and read appdemo.txt through TinyFS", "1.0",
      APP_TINYFS_TOOL, APP_AVAILABLE, app_tinyfs_demo, 0, "never_run" },
    { 4, "recovery-summary", "Summarize durable recovery state", "1.0",
      APP_RECOVERY_TOOL, APP_AVAILABLE, app_recovery_summary, 0, "never_run" },
    { 5, "task-demo", "Run one cooperative task demo round", "1.0",
      APP_DEMO, APP_AVAILABLE, app_task_demo, 0, "never_run" },
    { 6, "health-report", "Show current deterministic health score", "1.0",
      APP_DIAGNOSTIC, APP_AVAILABLE, app_health_report, 0, "never_run" },
};

static uint32_t app_total;
static struct app_log_entry app_log[APP_LOG_MAX];
static uint32_t app_log_total;
static uint32_t app_log_seq;
static uint32_t blocked_attempts;
static char last_app[APP_NAME_MAX];
static const char *last_result;
static char tinyfs_readback[TINYFS_MAX_FILE_SIZE + 1];

const char *app_type_name(enum app_type type)
{
    switch (type) {
    case APP_BUILTIN:       return "builtin";
    case APP_DIAGNOSTIC:    return "diagnostic";
    case APP_TINYFS_TOOL:   return "tinyfs_tool";
    case APP_RECOVERY_TOOL: return "recovery_tool";
    case APP_DEMO:          return "demo";
    }
    return "unknown";
}

const char *app_status_name(enum app_status status)
{
    switch (status) {
    case APP_AVAILABLE: return "available";
    case APP_RUNNING:   return "running";
    case APP_COMPLETED: return "completed";
    case APP_FAILED:    return "failed";
    case APP_BLOCKED:   return "blocked";
    }
    return "unknown";
}

static void serial_dec(uint32_t value)
{
    char buf[12];
    kitoa(value, buf, 10);
    serial_print(buf);
}

static void remember_last(const char *name, const char *result)
{
    kstrcpy(last_app, name && name[0] ? name : "none", sizeof(last_app));
    last_result = result;
}

static void append_log(const char *name, const char *result,
                       const char *status, const char *message)
{
    struct app_log_entry *e = &app_log[app_log_total % APP_LOG_MAX];
    e->seq = ++app_log_seq;
    kstrcpy(e->name, name && name[0] ? name : "unknown", sizeof(e->name));
    e->result = result;
    e->status = status;
    e->message = message;
    app_log_total++;
}

void apps_init(void)
{
    app_total = 6;
    app_log_total = 0;
    app_log_seq = 0;
    blocked_attempts = 0;
    remember_last("none", "none");
    for (uint32_t i = 0; i < app_total; i++) {
        apps[i].status = APP_AVAILABLE;
        apps[i].run_count = 0;
        apps[i].last_result = "never_run";
    }
    error_log(SEV_INFO, "APP-000", "apps",
              "Kernel-managed built-in app registry initialized",
              "not real userspace isolation");
}

uint32_t app_count(void)
{
    return app_total;
}

const struct app *app_get(uint32_t index)
{
    return index < app_total ? &apps[index] : 0;
}

const struct app *app_find(const char *name)
{
    for (uint32_t i = 0; i < app_total; i++)
        if (kstrcmp(apps[i].name, name) == 0)
            return &apps[i];
    return 0;
}

static struct app *mutable_app_find(const char *name)
{
    for (uint32_t i = 0; i < app_total; i++)
        if (kstrcmp(apps[i].name, name) == 0)
            return &apps[i];
    return 0;
}

int app_run(const char *name)
{
    const char *reason;
    struct app *app = mutable_app_find(name);
    if (!app_policy_can_run(app, name, &reason)) {
        blocked_attempts++;
        remember_last(name, "blocked");
        append_log(name, "blocked", "blocked", reason);
        error_log_class(SEV_WARNING, HEALTH_OPERATOR_INPUT, "APP-BLOCK-001", "apps",
                  "App run blocked by fixed registry policy",
                  "only compile-time built-in apps can run");
        return -1;
    }

    app->status = APP_RUNNING;
    remember_last(app->name, "running");
    append_log(app->name, "started", "running", "registered app entered");
    error_log(SEV_INFO, "APP-RUN-000", "apps",
              "Kernel-managed app run started",
              "entry is compile-time registered");

    int ok = app->entry();
    app->run_count++;
    app->status = ok ? APP_COMPLETED : APP_FAILED;
    app->last_result = ok ? "ok" : "failed";
    remember_last(app->name, app->last_result);
    append_log(app->name, app->last_result, app_status_name(app->status),
               ok ? "app completed" : "app returned failure");
    error_log(ok ? SEV_INFO : SEV_WARNING,
              ok ? "APP-RUN-001" : "APP-RUN-002",
              "apps",
              ok ? "Kernel-managed app completed" : "Kernel-managed app failed",
              "no process isolation or Ring 3 transition occurred");
    return ok ? 1 : 0;
}

uint32_t app_log_list(const struct app_log_entry **out, uint32_t capacity)
{
    if (!out)
        return 0;
    uint32_t available = app_log_total < APP_LOG_MAX ? app_log_total : APP_LOG_MAX;
    uint32_t count = available < capacity ? available : capacity;
    uint32_t first = app_log_total > APP_LOG_MAX ? app_log_total - APP_LOG_MAX : 0;
    for (uint32_t i = 0; i < count; i++)
        out[i] = &app_log[(first + i) % APP_LOG_MAX];
    return count;
}

const char *app_last_name(void)
{
    return last_app;
}

const char *app_last_result(void)
{
    return last_result;
}

uint32_t app_blocked_attempts(void)
{
    return blocked_attempts;
}

static int app_hello(void)
{
    vga_print("hello from KamikazeOS kernel-managed apps\n");
    vga_print("userland_mode=");
    vga_print(app_policy_mode());
    vga_print(" (not Ring 3 userspace)\n");
    return 1;
}

static int app_sysinfo(void)
{
    const struct module_version *kernel = module_get(0);
    vga_print("sysinfo:\n  kernel=");
    vga_print(kernel ? kernel->version : "?");
    vga_print("\n  ticks=");
    vga_print_dec(timer_ticks());
    vga_print("\n  tasks=");
    vga_print_dec(task_count());
    vga_print("\n  app_model=compile_time_registry\n");
    return 1;
}

static int tinyfs_file_exists(const char *name)
{
    const struct tinyfs_file *list[TINYFS_MAX_FILES];
    uint32_t count = tinyfs_list(list, TINYFS_MAX_FILES);
    for (uint32_t i = 0; i < count; i++)
        if (kstrcmp(list[i]->name, name) == 0)
            return 1;
    return 0;
}

static int app_tinyfs_demo(void)
{
    const char *name = "appdemo.txt";
    const char *msg = "hello-from-app";
    uint32_t size = 0;
    if (!tinyfs_status()->formatted) {
        vga_print("tinyfs-demo: TinyFS is unavailable or not formatted\n");
        return 0;
    }
    if (!tinyfs_file_exists(name) && !tinyfs_create(name)) {
        vga_print("tinyfs-demo: create failed\n");
        return 0;
    }
    if (!tinyfs_write_text(name, msg)) {
        vga_print("tinyfs-demo: write failed\n");
        return 0;
    }
    if (!tinyfs_read_text(name, tinyfs_readback, sizeof(tinyfs_readback), &size)) {
        vga_print("tinyfs-demo: readback failed\n");
        return 0;
    }
    vga_print("tinyfs-demo: appdemo.txt -> ");
    vga_print(tinyfs_readback);
    vga_print(" (");
    vga_print_dec(size);
    vga_print(" bytes)\n");
    return kstrcmp(tinyfs_readback, msg) == 0;
}

static int app_recovery_summary(void)
{
    const struct recovery_memory *m = recovery_memory_get();
    vga_print("recovery-summary:\n  backend=");
    vga_print(recovery_memory_backend());
    vga_print("\n  boot_count=");
    vga_print_dec(m->boot_count);
    vga_print("\n  journal_entries=");
    vga_print_dec(recovery_journal_count());
    vga_print("\n  previous_panic=");
    vga_print(m->prev_panic_detected ? "yes\n" : "no\n");
    return 1;
}

static int app_task_demo(void)
{
    uint32_t ran = task_demo_round();
    vga_print("task-demo app ran ");
    vga_print_dec(ran);
    vga_print(" cooperative task step(s)\n");
    return 1;
}

static int app_health_report(void)
{
    struct health_breakdown hb;
    boot_snapshot_health_now(&hb);
    vga_print("health-report: score=");
    vga_print_dec((uint32_t)hb.score);
    vga_print("/100 missing_penalty=");
    vga_print_dec((uint32_t)(-hb.missing_penalty));
    vga_print(" isolated_penalty=");
    vga_print_dec((uint32_t)(-hb.isolated_penalty));
    vga_print("\n");
    return 1;
}

void app_export_report(void)
{
    serial_print("[APPS]\n");
    serial_print("total_apps="); serial_dec(app_total);
    serial_print("\nuserland_mode="); serial_print(app_policy_mode());
    serial_print("\nlast_app="); serial_print(last_app);
    serial_print("\nlast_result="); serial_print(last_result);
    serial_print("\nblocked_attempts="); serial_dec(blocked_attempts);
    serial_print("\n");
    for (uint32_t i = 0; i < app_total; i++) {
        serial_print("app_id="); serial_dec(apps[i].app_id);
        serial_print(";name="); serial_print(apps[i].name);
        serial_print(";description="); serial_print(apps[i].description);
        serial_print(";version="); serial_print(apps[i].version);
        serial_print(";type="); serial_print(app_type_name(apps[i].type));
        serial_print(";status="); serial_print(app_status_name(apps[i].status));
        serial_print(";run_count="); serial_dec(apps[i].run_count);
        serial_print(";last_result="); serial_print(apps[i].last_result);
        serial_print("\n");
    }

    serial_print("[APP_LOG]\n");
    const struct app_log_entry *rows[APP_LOG_MAX];
    uint32_t count = app_log_list(rows, APP_LOG_MAX);
    for (uint32_t i = 0; i < count; i++) {
        serial_print("seq="); serial_dec(rows[i]->seq);
        serial_print(";name="); serial_print(rows[i]->name);
        serial_print(";result="); serial_print(rows[i]->result);
        serial_print(";status="); serial_print(rows[i]->status);
        serial_print(";message="); serial_print(rows[i]->message);
        serial_print("\n");
    }
}
