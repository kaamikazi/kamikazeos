# Kernel Syscalls

KamikazeOS v2.4 "Syscall Door" introduced a controlled kernel service
interface. v2.5 made its IDT gate DPL3-capable, and v2.6 prepares it for one
fixed linked routine after strict privilege and page-permission validation.
One approved disposable run verified two CPL3 calls and the controlled exit;
the default one-shot execution gate is held after that run.

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
| 9 | `SYS_USER_EXIT` | CPL3 result code; controlled return only |

Unknown IDs and invalid indexes fail closed and are logged. The TinyFS calls
are read-only. No entry can approve or apply a package, change policy, write an
arbitrary disk sector, run an unknown app, or access arbitrary memory.

CPL3 callers are restricted to IDs 2, 3, 8, and 9. `SYS_USER_EXIT` is rejected
from CPL0. Every dispatch validates exact saved code/data selectors and
records caller CPL, call ID, result, kernel entry ESP, and whether the ESP0
transition stack was observed.

## Initialization And Reports

The exception IDT and TSS are loaded early. Paging and core services initialize
before the syscall subsystem installs vector `0x80`; IRQ setup remains
separate. DPL3 is selected only when user descriptors, TSS loading, bounded
ESP0, and paging all validate. The
`[SYSCALLS]` report records gate state and counters, while `[SYSCALL_TABLE]`
describes the fixed registry. The analyzer and Recovery Center only read these
records.

## Current Boundary

Real now: the DPL3-capable IDT gate, assembly stub with segment/register
preservation, fixed dispatcher, semantic caller-origin policy, defensive
self-tests, statistics, reports, analyzer, and dashboard.

Verified for the fixed linked routine: a TSS stack switch from CPL3, exact
saved-selector origin validation, and controlled CPL3 exit. Additional runs
remain approval-gated.

Not implemented: userspace pointers, user/kernel copy helpers, process
isolation, per-process permissions, executable loading, ELF, syscalls from
apps, multiple processes, or ABI stability guarantees.
