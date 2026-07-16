#include "syscall.h"
#include "app.h"
#include "boot_snapshot.h"
#include "errors.h"
#include "gdt.h"
#include "idt.h"
#include "interrupts.h"
#include "paging.h"
#include "ring3.h"
#include "string.h"
#include "task.h"
#include "timer.h"
#include "tinyfs.h"
#include "tss.h"
#include "vga.h"

typedef int32_t (*syscall_handler)(uint32_t, uint32_t, uint32_t, uint32_t *);

struct syscall_entry {
    struct syscall_descriptor descriptor;
    syscall_handler handler;
};

static int32_t handle_write_console(uint32_t, uint32_t, uint32_t, uint32_t *);
static int32_t handle_get_ticks(uint32_t, uint32_t, uint32_t, uint32_t *);
static int32_t handle_get_health(uint32_t, uint32_t, uint32_t, uint32_t *);
static int32_t handle_list_files(uint32_t, uint32_t, uint32_t, uint32_t *);
static int32_t handle_read_file(uint32_t, uint32_t, uint32_t, uint32_t *);
static int32_t handle_app_info(uint32_t, uint32_t, uint32_t, uint32_t *);
static int32_t handle_yield(uint32_t, uint32_t, uint32_t, uint32_t *);
static int32_t handle_system_info(uint32_t, uint32_t, uint32_t, uint32_t *);
static int32_t handle_user_exit(uint32_t, uint32_t, uint32_t, uint32_t *);

static const struct syscall_entry table[] = {
    {{SYS_WRITE_CONSOLE, "write_console", "message_token"}, handle_write_console},
    {{SYS_GET_TICKS, "get_ticks", "none"}, handle_get_ticks},
    {{SYS_GET_HEALTH, "get_health", "none"}, handle_get_health},
    {{SYS_LIST_FILES, "list_files", "file_index"}, handle_list_files},
    {{SYS_READ_FILE, "read_file", "file_index"}, handle_read_file},
    {{SYS_APP_INFO, "app_info", "app_index"}, handle_app_info},
    {{SYS_YIELD, "yield", "none"}, handle_yield},
    {{SYS_SYSTEM_INFO, "system_info", "none"}, handle_system_info},
    {{SYS_USER_EXIT, "user_exit", "result_code_cpl3_only"}, handle_user_exit},
};

static struct syscall_stats stats;
static char last_text[SYSCALL_TEXT_MAX + 1u];

static const struct syscall_entry *find_entry(uint32_t id)
{
    for (uint32_t i = 0; i < syscall_table_count(); i++)
        if (table[i].descriptor.id == id)
            return &table[i];
    return 0;
}

static void set_text(const char *text)
{
    kstrcpy(last_text, text ? text : "", sizeof(last_text));
}

static int args_are_zero(uint32_t a, uint32_t b, uint32_t c)
{
    return a == 0 && b == 0 && c == 0;
}

static int32_t handle_write_console(uint32_t token, uint32_t b, uint32_t c,
                                    uint32_t *value)
{
    if (token != 1 || b != 0 || c != 0)
        return SYSCALL_ERR_INVALID_ARG;
    const char *message = "[syscall] controlled console write\n";
    vga_print(message);
    set_text(message);
    *value = (uint32_t)kstrlen(message);
    return SYSCALL_OK;
}

static int32_t handle_get_ticks(uint32_t a, uint32_t b, uint32_t c,
                                uint32_t *value)
{
    if (!args_are_zero(a, b, c))
        return SYSCALL_ERR_INVALID_ARG;
    *value = timer_ticks();
    set_text("timer_ticks");
    return timer_is_enabled() ? SYSCALL_OK : SYSCALL_ERR_UNAVAILABLE;
}

static int32_t handle_get_health(uint32_t a, uint32_t b, uint32_t c,
                                 uint32_t *value)
{
    if (!args_are_zero(a, b, c))
        return SYSCALL_ERR_INVALID_ARG;
    struct health_breakdown health;
    boot_snapshot_health_now(&health);
    *value = (uint32_t)health.score;
    set_text("health_score");
    return SYSCALL_OK;
}

static int32_t get_file(uint32_t index, const struct tinyfs_file **file)
{
    if (!tinyfs_status()->formatted)
        return SYSCALL_ERR_UNAVAILABLE;
    const struct tinyfs_file *files[TINYFS_MAX_FILES];
    uint32_t count = tinyfs_list(files, TINYFS_MAX_FILES);
    if (index >= count)
        return SYSCALL_ERR_INVALID_ARG;
    *file = files[index];
    return SYSCALL_OK;
}

static int32_t handle_list_files(uint32_t index, uint32_t b, uint32_t c,
                                 uint32_t *value)
{
    if (b != 0 || c != 0)
        return SYSCALL_ERR_INVALID_ARG;
    const struct tinyfs_file *file;
    int32_t result = get_file(index, &file);
    if (result != SYSCALL_OK)
        return result;
    set_text(file->name);
    *value = file->size;
    return SYSCALL_OK;
}

static int32_t handle_read_file(uint32_t index, uint32_t b, uint32_t c,
                                uint32_t *value)
{
    if (b != 0 || c != 0)
        return SYSCALL_ERR_INVALID_ARG;
    const struct tinyfs_file *file;
    int32_t result = get_file(index, &file);
    if (result != SYSCALL_OK)
        return result;
    uint32_t size = 0;
    if (!tinyfs_read_text(file->name, last_text, sizeof(last_text), &size))
        return SYSCALL_ERR_UNAVAILABLE;
    *value = size;
    return SYSCALL_OK;
}

static int32_t handle_app_info(uint32_t index, uint32_t b, uint32_t c,
                               uint32_t *value)
{
    if (b != 0 || c != 0 || index >= app_count())
        return SYSCALL_ERR_INVALID_ARG;
    const struct app *app = app_get(index);
    if (!app)
        return SYSCALL_ERR_UNAVAILABLE;
    set_text(app->name);
    *value = app->app_id;
    return SYSCALL_OK;
}

static int32_t handle_yield(uint32_t a, uint32_t b, uint32_t c,
                            uint32_t *value)
{
    if (!args_are_zero(a, b, c))
        return SYSCALL_ERR_INVALID_ARG;
    task_yield();
    *value = task_current() ? task_current()->task_id : 0;
    set_text("cooperative_yield_complete");
    return SYSCALL_OK;
}

static int32_t handle_system_info(uint32_t a, uint32_t b, uint32_t c,
                                  uint32_t *value)
{
    if (!args_are_zero(a, b, c))
        return SYSCALL_ERR_INVALID_ARG;
    uint32_t flags = 0;
    if (paging_is_enabled()) flags |= 1u;
    if (timer_is_enabled()) flags |= 2u;
    if (tinyfs_status()->formatted) flags |= 4u;
    *value = flags;
    set_text("KamikazeOS 2.4 kernel_internal_syscalls");
    return SYSCALL_OK;
}

static int32_t handle_user_exit(uint32_t a, uint32_t b, uint32_t c,
                                uint32_t *value)
{
    (void)a; (void)b; (void)c; (void)value;
    return SYSCALL_ERR_DENIED;
}

static int syscall_origin_allowed(uint32_t id, uint8_t cpl)
{
    if (cpl == 0u)
        return id != SYS_USER_EXIT;
    if (cpl != 3u)
        return 0;
    return id == SYS_GET_TICKS || id == SYS_GET_HEALTH ||
           id == SYS_SYSTEM_INFO || id == SYS_USER_EXIT;
}

void syscall_init(void)
{
    kmemset(&stats, 0, sizeof(stats));
    kmemset(last_text, 0, sizeof(last_text));
    stats.vector = SYSCALL_VECTOR;
    stats.registered_count = syscall_table_count();
    stats.last_id = 0xFFFFFFFFu;
    stats.last_result = SYSCALL_OK;
    stats.user_mode_available = 0;
    uint8_t requested_dpl = (gdt_user_segments_valid() &&
                             tss_is_loaded() &&
                             tss_kernel_stack_valid() &&
                             paging_is_enabled()) ? 3u : 0u;
    idt_install_syscall_gate(requested_dpl);
    stats.initialized = idt_syscall_gate_installed();
    stats.gate_dpl = idt_syscall_gate_dpl();
    stats.syscall_user_gate = stats.initialized && stats.gate_dpl == 3u;
    error_log(stats.initialized ? SEV_INFO : SEV_ERROR,
              stats.initialized ? "SYS-000" : "SYS-ERR-003", "Syscalls",
              stats.initialized ? (stats.syscall_user_gate
                  ? "Kernel syscall gate initialized at vector 128 DPL3"
                  : "Kernel syscall gate initialized at vector 128 DPL0 fallback")
                                : "Kernel syscall gate initialization failed",
              stats.initialized ? (stats.syscall_user_gate
                  ? "Fixed CPL3 test still requires the final user-memory gate"
                  : "TSS or user segment prerequisite unavailable")
                                : "do not invoke syscalls");
}

void syscall_dispatch(struct interrupt_frame *frame)
{
    uint32_t id = frame ? frame->eax : 0xFFFFFFFFu;
    uint32_t value = 0;
    int32_t result = SYSCALL_ERR_UNAVAILABLE;
    if (!frame)
        return;
    stats.total_calls++;
    stats.last_id = id;
    uint8_t caller_cpl = (uint8_t)(frame->cs & 3u);
    stats.last_caller_cpl = caller_cpl;
    stats.last_kernel_esp = (uint32_t)frame;
    if (caller_cpl == 0u) stats.cpl0_calls++;
    else if (caller_cpl == 3u) stats.cpl3_calls++;
    ring3_note_syscall(caller_cpl, id, (uint32_t)frame);
    if (caller_cpl == 3u && (uint32_t)frame >= tss_privilege_stack_bottom() &&
        (uint32_t)frame < tss_privilege_stack_top())
        stats.esp0_switch_observed = 1;
    last_text[0] = '\0';
    const struct syscall_entry *entry = find_entry(id);
    if (!stats.initialized) {
        result = SYSCALL_ERR_UNAVAILABLE;
    } else if (!entry) {
        result = SYSCALL_ERR_UNKNOWN;
        error_log(SEV_WARNING, "SYS-ERR-001", "Syscalls",
                  "Unknown syscall ID rejected", "use the fixed syscall table");
    } else if (!syscall_origin_allowed(id, caller_cpl)) {
        result = SYSCALL_ERR_DENIED;
        stats.origin_denied_calls++;
        error_log(SEV_WARNING, "SYS-ERR-004", "Syscalls",
                  "Syscall origin policy rejected caller CPL",
                  "use only services permitted for the saved CS privilege");
    } else if (id == SYS_USER_EXIT) {
        if (frame->ebx > 2u || frame->ecx != 0 || frame->edx != 0) {
            result = SYSCALL_ERR_INVALID_ARG;
        } else {
            stats.last_result = SYSCALL_OK;
            stats.last_value = frame->ebx;
            ring3_complete_user_exit(frame->ebx, (uint32_t)frame);
        }
    } else {
        result = entry->handler(frame->ebx, frame->ecx, frame->edx, &value);
        if (result != SYSCALL_OK)
            error_log(SEV_WARNING, "SYS-ERR-002", "Syscalls",
                      "Syscall arguments or service state rejected",
                      "use bounded handles and documented arguments");
    }
    if (result != SYSCALL_OK)
        stats.failed_calls++;
    stats.last_result = result;
    stats.last_value = value;
    frame->eax = (uint32_t)result;
    frame->edx = value;
}

int32_t syscall_invoke_test(uint32_t id, uint32_t arg0, uint32_t arg1,
                            uint32_t arg2, uint32_t *value_out)
{
    register uint32_t eax __asm__("eax") = id;
    register uint32_t ebx __asm__("ebx") = arg0;
    register uint32_t ecx __asm__("ecx") = arg1;
    register uint32_t edx __asm__("edx") = arg2;
    __asm__ volatile ("int $0x80"
                      : "+a"(eax), "+d"(edx)
                      : "b"(ebx), "c"(ecx)
                      : "memory", "cc");
    if (value_out)
        *value_out = edx;
    return (int32_t)eax;
}

const char *syscall_name(uint32_t id)
{
    const struct syscall_entry *entry = find_entry(id);
    return entry ? entry->descriptor.name : "unknown";
}

const char *syscall_result_name(int32_t result)
{
    switch (result) {
    case SYSCALL_OK: return "ok";
    case SYSCALL_ERR_UNKNOWN: return "unknown";
    case SYSCALL_ERR_INVALID_ARG: return "invalid_arg";
    case SYSCALL_ERR_DENIED: return "denied";
    case SYSCALL_ERR_UNAVAILABLE: return "unavailable";
    default: return "invalid_result";
    }
}

void syscall_stats(struct syscall_stats *out)
{
    if (out)
        *out = stats;
}

uint32_t syscall_table_count(void)
{
    return (uint32_t)(sizeof(table) / sizeof(table[0]));
}

const struct syscall_descriptor *syscall_table_get(uint32_t index)
{
    return index < syscall_table_count() ? &table[index].descriptor : 0;
}

const char *syscall_last_text(void)
{
    return last_text;
}
