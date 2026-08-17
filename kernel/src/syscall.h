#ifndef KMZ_SYSCALL_H
#define KMZ_SYSCALL_H

#include <stdint.h>

#define SYSCALL_VECTOR 0x80u
#define SYSCALL_TEXT_MAX 2048u

enum syscall_id {
    SYS_WRITE_CONSOLE = 1,
    SYS_GET_TICKS = 2,
    SYS_GET_HEALTH = 3,
    SYS_LIST_FILES = 4,
    SYS_READ_FILE = 5,
    SYS_APP_INFO = 6,
    SYS_YIELD = 7,
    SYS_SYSTEM_INFO = 8,
    SYS_USER_EXIT = 9,
};

enum syscall_result {
    SYSCALL_OK = 0,
    SYSCALL_ERR_UNKNOWN = -1,
    SYSCALL_ERR_INVALID_ARG = -2,
    SYSCALL_ERR_DENIED = -3,
    SYSCALL_ERR_UNAVAILABLE = -4,
};

enum syscall_origin {
    SYSCALL_ORIGIN_INVALID = 0,
    SYSCALL_ORIGIN_KERNEL = 1,
    SYSCALL_ORIGIN_USER = 2,
};

struct interrupt_frame;

struct syscall_descriptor {
    uint32_t id;
    const char *name;
    const char *argument_mode;
};

struct syscall_stats {
    int initialized;
    uint32_t vector;
    uint32_t registered_count;
    uint32_t total_calls;
    uint32_t failed_calls;
    uint32_t last_id;
    int32_t last_result;
    uint32_t last_value;
    int user_mode_available;
    int syscall_user_gate;
    uint8_t gate_dpl;
    uint8_t last_caller_cpl;
    uint8_t last_origin;
    int last_origin_valid;
    uint32_t last_cs;
    uint32_t last_ds;
    uint32_t last_es;
    uint32_t last_fs;
    uint32_t last_gs;
    uint32_t cpl0_calls;
    uint32_t cpl3_calls;
    uint32_t origin_denied_calls;
    uint32_t last_kernel_esp;
    int esp0_switch_observed;
    int origin_policy_self_test;
    int register_preservation_test;
};

void syscall_init(void);
void syscall_dispatch(struct interrupt_frame *frame);
int32_t syscall_invoke_test(uint32_t id, uint32_t arg0, uint32_t arg1,
                            uint32_t arg2, uint32_t *value_out);
const char *syscall_name(uint32_t id);
const char *syscall_result_name(int32_t result);
const char *syscall_origin_name(uint8_t origin);
void syscall_stats(struct syscall_stats *out);
uint32_t syscall_table_count(void);
const struct syscall_descriptor *syscall_table_get(uint32_t index);
const char *syscall_last_text(void);

#endif
