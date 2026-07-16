# Kernel Syscalls

KamikazeOS v2.4 "Syscall Door" introduced a controlled kernel-internal system
call interface. v2.5 "Privilege Boundary" makes its IDT gate DPL3-capable after
strict privilege prerequisites pass. No Ring 3 caller exists yet.

## ABI

- Entry: `int 0x80` through a DPL3 interrupt gate when privilege readiness
  validates; DPL0 fallback otherwise.
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

The exception IDT and TSS are loaded early. Paging and core services initialize
before the syscall subsystem installs vector `0x80`; IRQ setup remains
separate. DPL3 is selected only when user descriptors, TSS loading, bounded
ESP0, and paging all validate. The
`[SYSCALLS]` report records gate state and counters, while `[SYSCALL_TABLE]`
describes the fixed registry. The analyzer and Recovery Center only read these
records.

## Current Boundary

Real: the DPL3-capable IDT gate, assembly stub with segment/register
preservation, register ABI, fixed dispatcher, validation, statistics, shell
tests, reports, analyzer, and dashboard. Current test calls still originate in
Ring 0.

Not implemented: Ring 3 callers, userspace pointers, process isolation,
per-process permissions, executable loading, ELF, syscalls from apps, or ABI
stability guarantees.
