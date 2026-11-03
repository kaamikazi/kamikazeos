#ifndef KMZ_TASK_H
#define KMZ_TASK_H

#include <stdint.h>

#define TASK_MAX 8
#define TASK_NAME_MAX 24

enum task_state {
    TASK_READY = 0,
    TASK_RUNNING,
    TASK_SLEEPING,
    TASK_BLOCKED,
    TASK_FINISHED,
    TASK_CRASHED,
};

enum task_flags {
    TASK_FLAG_SYSTEM = 1u << 0,
    TASK_FLAG_DEMO   = 1u << 1,
};

typedef void (*task_entry_fn)(void);

struct task {
    uint32_t task_id;
    char name[TASK_NAME_MAX];
    enum task_state state;
    uint8_t priority;
    uint32_t ticks_run;
    uint32_t created_at_tick;
    uint32_t last_run_tick;
    task_entry_fn entry;
    uint32_t flags;
};

void task_init(void);
int task_create(const char *name, uint8_t priority,
                task_entry_fn entry, uint32_t flags);
void task_yield(void);
void task_tick(void);
uint32_t task_list(const struct task **out, uint32_t capacity);
int task_set_state(uint32_t task_id, enum task_state state);
const struct task *task_current(void);
const char *task_state_name(enum task_state state);
uint32_t task_count(void);
uint32_t task_count_state(enum task_state state);
uint32_t task_demo_round(void);
uint32_t task_idle_steps(void);
uint32_t task_health_steps(void);
uint32_t task_recovery_steps(void);

#endif
