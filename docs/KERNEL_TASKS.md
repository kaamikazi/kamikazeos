# Kernel Tasks

KamikazeOS v1.7.0 "Task Seed" adds a small, fixed kernel task model and a
cooperative scheduler skeleton. It is a foundation for reasoning about work,
not a process system.

## Fixed Task Table

`kernel/src/task.c` owns a statically allocated table of at most eight tasks.
There is no heap allocation. Each task record contains:

- numeric task ID and fixed-size name
- state and priority
- ticks run, creation tick, and last-run tick
- a kernel-owned step function pointer
- fixed flags

States are `ready`, `running`, `sleeping`, `blocked`, `finished`, and
`crashed`.

## Built-In Tasks

The initial table contains:

| Task | Role |
|---|---|
| `kernel_shell` | Represents the current kernel execution owner |
| `idle_task` | Increments an idle-step counter |
| `health_monitor` | Increments a health-monitor counter |
| `recovery_watch` | Increments a recovery-watch counter |

The three demo tasks update counters only. They do not write recovery state,
apply actions, modify packages, access networks, or load code.

## Cooperative Scheduling

`task_yield()` temporarily selects one ready built-in task, invokes its small
step function on the existing kernel stack, and returns control to
`kernel_shell`. `task-demo` performs one three-step round.

There are no saved register contexts, task stacks, privilege changes, or
timer-driven switches. PIT IRQ0 calls `task_tick()` only to account ticks to
the current logical task.

## Commands

```text
tasks
task-current
task-yield
task-demo
```

## Reporting

`[KERNEL_TASKS]` reports scheduler mode, current task, state counts, and one
row per task. The offline analyzer and Recovery Center consume this data
read-only and only warn when blocked or crashed records are present.

## Not Implemented

- userspace or privilege isolation
- process creation or arbitrary task loading
- per-task stacks
- register context switching
- preemptive scheduling
- virtual memory or per-process address spaces
- syscalls, IPC, or process termination semantics
