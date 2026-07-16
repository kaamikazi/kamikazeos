# Kernel Syscalls

KamikazeOS v2.4 "Syscall Door" introduces a controlled kernel-internal system
call interface. It is foundation work for a possible future privilege boundary,
not a claim of Ring 3 userspace.

## ABI

- Entry: `int 0x80` through a DPL0 interrupt gate.
- Input: EAX call ID; EBX, ECX, and EDX bounded scalar/index arguments.
- Output: EAX signed result code; EDX scalar result value.
- Text: fixed kernel-owned output buffer, visible only through kernel APIs.
- Raw caller pointers: not supported.

Result codes are `SYSCALL_OK`, `SYSCALL_ERR_UNKNOWN`,
`SYSCALL_ERR_INVALID_ARG`, `SYSCALL_ERR_DENIED`, and
`SYSCALL_ERR_UNAVAILABLE`.

## Fixed Registry

| ID | Name | Argument |
|---:|---|---|
| 1 | `SYS_WRITE_CONSOLE` | Fixed message token |
| 2 | `SYS_GET_TICKS` | None |
| 3 | `SYS_GET_HEALTH` | None |
| 4 | `SYS_LIST_FILES` | TinyFS file index |
| 5 | `SYS_READ_FILE` | TinyFS file index |
| 6 | `SYS_APP_INFO` | Built-in app index |
| 7 | `SYS_YIELD` | None |
| 8 | `SYS_SYSTEM_INFO` | None |

Unknown IDs and invalid indexes fail closed and are logged. The TinyFS calls
are read-only. No entry can approve or apply a package, change policy, write an
arbitrary disk sector, run an unknown app, or access arbitrary memory.

## Initialization And Reports

The exception IDT is loaded early. Paging and core services initialize before
the syscall subsystem installs vector `0x80`; IRQ setup remains separate. The
`[SYSCALLS]` report records gate state and counters, while `[SYSCALL_TABLE]`
describes the fixed registry. The analyzer and Recovery Center only read these
records.

## Current Boundary

Real: the IDT gate, assembly stub, register ABI, fixed dispatcher, validation,
statistics, shell tests, reports, analyzer, and dashboard.

Not implemented: Ring 3 callers, userspace pointers, process isolation,
per-process permissions, executable loading, ELF, syscalls from apps, or ABI
stability guarantees.
