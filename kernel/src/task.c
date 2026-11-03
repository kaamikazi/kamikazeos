#include "task.h"
#include "errors.h"
#include "string.h"
#include "timer.h"

static struct task tasks[TASK_MAX];
static uint32_t task_total;
static uint32_t current_index;
static uint32_t schedule_cursor;
static uint32_t next_task_id;
static uint32_t idle_steps;
static uint32_t health_steps;
static uint32_t recovery_steps;

static void idle_task(void)
{
    idle_steps++;
}

static void health_monitor_task(void)
{
    health_steps++;
}

static void recovery_watch_task(void)
{
    recovery_steps++;
}

const char *task_state_name(enum task_state state)
{
    switch (state) {
    case TASK_READY:    return "ready";
    case TASK_RUNNING:  return "running";
    case TASK_SLEEPING: return "sleeping";
    case TASK_BLOCKED:  return "blocked";
    case TASK_FINISHED: return "finished";
    case TASK_CRASHED:  return "crashed";
    }
    return "unknown";
}

int task_create(const char *name, uint8_t priority,
                task_entry_fn entry, uint32_t flags)
{
    if (!name || !name[0] || task_total >= TASK_MAX)
        return -1;

    struct task *t = &tasks[task_total++];
    t->task_id = next_task_id++;
    kstrcpy(t->name, name, TASK_NAME_MAX);
    t->state = TASK_READY;
    t->priority = priority;
    t->ticks_run = 0;
    t->created_at_tick = timer_ticks();
    t->last_run_tick = 0;
    t->entry = entry;
    t->flags = flags;
    return (int)t->task_id;
}

void task_init(void)
{
    kmemset(tasks, 0, sizeof(tasks));
    task_total = 0;
    current_index = 0;
    schedule_cursor = 0;
    next_task_id = 1;
    idle_steps = health_steps = recovery_steps = 0;

    task_create("kernel_shell", 3, 0, TASK_FLAG_SYSTEM);
    task_create("idle_task", 0, idle_task, TASK_FLAG_SYSTEM | TASK_FLAG_DEMO);
    task_create("health_monitor", 2, health_monitor_task,
                TASK_FLAG_SYSTEM | TASK_FLAG_DEMO);
    task_create("recovery_watch", 2, recovery_watch_task,
                TASK_FLAG_SYSTEM | TASK_FLAG_DEMO);
    tasks[0].state = TASK_RUNNING;
    tasks[0].last_run_tick = timer_ticks();

    error_log(SEV_INFO, "TSK-000", "tasks",
              "Fixed cooperative task table initialized",
              "timer accounts ticks but never switches stacks");
}

void task_tick(void)
{
    if (task_total == 0 || current_index >= task_total)
        return;
    tasks[current_index].ticks_run++;
    tasks[current_index].last_run_tick = timer_ticks();
}

void task_yield(void)
{
    if (task_total < 2)
        return;

    uint32_t flags;
    __asm__ volatile ("pushfl; popl %0" : "=r"(flags));
    __asm__ volatile ("cli");
    uint32_t caller = current_index;
    uint32_t selected = TASK_MAX;
    for (uint32_t offset = 1; offset <= task_total; offset++) {
        uint32_t candidate = (schedule_cursor + offset) % task_total;
        if (candidate != caller && tasks[candidate].state == TASK_READY) {
            selected = candidate;
            break;
        }
    }

    if (selected == TASK_MAX) {
        if (flags & (1u << 9))
            __asm__ volatile ("sti");
        return;
    }

    tasks[caller].state = TASK_READY;
    tasks[selected].state = TASK_RUNNING;
    tasks[selected].last_run_tick = timer_ticks();
    current_index = selected;
    schedule_cursor = selected;
    task_entry_fn entry = tasks[selected].entry;
    if (flags & (1u << 9))
        __asm__ volatile ("sti");

    if (entry)
        entry();

    __asm__ volatile ("cli");
    if (tasks[selected].state == TASK_RUNNING)
        tasks[selected].state = TASK_READY;
    if (tasks[caller].state == TASK_READY)
        tasks[caller].state = TASK_RUNNING;
    current_index = caller;
    tasks[caller].last_run_tick = timer_ticks();
    if (flags & (1u << 9))
        __asm__ volatile ("sti");
}

uint32_t task_list(const struct task **out, uint32_t capacity)
{
    if (!out)
        return 0;
    uint32_t n = task_total < capacity ? task_total : capacity;
    for (uint32_t i = 0; i < n; i++)
        out[i] = &tasks[i];
    return n;
}

int task_set_state(uint32_t task_id, enum task_state state)
{
    if (state < TASK_READY || state > TASK_CRASHED)
        return 0;
    for (uint32_t i = 0; i < task_total; i++) {
        if (tasks[i].task_id != task_id)
            continue;
        if (i == current_index && state != TASK_RUNNING)
            return 0;
        tasks[i].state = state;
        return 1;
    }
    return 0;
}

const struct task *task_current(void)
{
    return task_total ? &tasks[current_index] : 0;
}

uint32_t task_count(void)
{
    return task_total;
}

uint32_t task_count_state(enum task_state state)
{
    uint32_t count = 0;
    for (uint32_t i = 0; i < task_total; i++)
        if (tasks[i].state == state)
            count++;
    return count;
}

uint32_t task_demo_round(void)
{
    uint32_t before = idle_steps + health_steps + recovery_steps;
    task_yield();
    task_yield();
    task_yield();
    return idle_steps + health_steps + recovery_steps - before;
}

uint32_t task_idle_steps(void) { return idle_steps; }
uint32_t task_health_steps(void) { return health_steps; }
uint32_t task_recovery_steps(void) { return recovery_steps; }
