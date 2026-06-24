# Apps And Userland Boundary

Status: v1.9.0 "Userland Spark".

KamikazeOS v1.9 adds the first userland-style program model without claiming
real userspace isolation. Apps are fixed, kernel-managed built-ins registered
at compile time. The `run <app>` shell command dispatches only through that
registry and the app policy gate.

## What Is Real

- A fixed app registry in `kernel/src/app.c`.
- A policy gate in `kernel/src/app_policy.c`.
- Shell commands: `apps`, `app <name>`, `run <name>`, and `app-log`.
- App run counts and last-result tracking.
- Runtime app run/block history.
- Serial report sections: `[APPS]` and `[APP_LOG]`.
- Analyzer and Recovery Center visibility.
- A TinyFS-backed demo app that writes and reads `appdemo.txt` through the
  existing TinyFS API.

## Built-In Apps

| App | Type | Behavior |
|---|---|---|
| `hello` | demo | Prints a greeting and the app model boundary |
| `sysinfo` | diagnostic | Shows kernel version, ticks, and task count |
| `tinyfs-demo` | tinyfs_tool | Creates/writes/reads `appdemo.txt` |
| `recovery-summary` | recovery_tool | Shows durable recovery state summary |
| `task-demo` | demo | Runs one cooperative task demo round |
| `health-report` | diagnostic | Shows the deterministic health score |

## Safety Rules

- Only fixed built-in apps can run.
- Unknown app names are blocked and logged.
- TinyFS files are never treated as executable code.
- Package payloads are never executed.
- App entry points must be registered at compile time.
- Every run or blocked attempt is logged.

## Not Real Yet

- No Ring 3 userspace.
- No process isolation.
- No executable loading.
- No ELF loader.
- No per-process memory.
- No syscalls.
- No virtual memory.

The v1.9 app model is a control-plane milestone: it gives KamikazeOS a visible
program registry and a safe launch policy without opening arbitrary execution.
