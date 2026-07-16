# Controlled Ring 3 Execution

KamikazeOS v2.6 "Ring 3 Seed" performs one real x86 CPL0-to-CPL3 transition.
The target is a fixed trusted routine linked into the kernel image at build
time. Nothing is loaded or executed from TinyFS, packages, or host input.

## Transition Design

`ring3_enter_user()` saves the original Ring 0 flags and registers, records the
original kernel ESP, loads the DPL3 data selectors, and constructs this
`iretd` frame:

```text
SS      0x23 (user data, RPL3)
ESP     fixed top of .userstack
EFLAGS  0x202 (IF enabled deliberately, IOPL 0)
CS      0x1B (user code, RPL3)
EIP     fixed symbol in .usertext
```

Interrupts remain enabled so PIT and keyboard IRQ handling continue. IOPL is
zero, so CPL3 cannot perform port I/O or change interrupt state with privileged
instructions.

The test routine calls `SYS_GET_TICKS`, then `SYS_USER_EXIT`. On `int 0x80`,
the CPU uses TSS `SS0:ESP0` to enter the dedicated 16 KiB kernel transition
stack. The syscall stub preserves general and segment registers and switches
to kernel data selectors before dispatching C code.

`SYS_USER_EXIT` is accepted only when the saved CS reports CPL3. It records the
result, abandons the temporary ESP0 interrupt frame by design, restores the
saved original Ring 0 context, and returns to the shell. The shell never runs
on the user or ESP0 stack.

## User Memory

The linker reserves aligned `.usertext` and `.userstack` sections. Exact
addresses are reported by `user-memory` and `[USER_MEMORY]` because they may
move when the linked image changes.

- code: one 4 KiB identity-mapped user/read-only page
- stack: four 4 KiB identity-mapped user/writable pages
- stack growth: none
- caller-selected stack address: none
- guard page: none

The enclosing PDE must carry the x86 USER bit, but only these five PTEs do.
Validation scans all 0-16 MiB PTEs and requires exactly five user pages. Low
memory, VGA, the privileged kernel image, static buffers, heap, GDT/IDT/TSS,
bootstrap and ESP0 stacks, page directory, and page tables remain supervisor
PTEs. A mismatch leaves `execution_enabled=no`.

## Syscall Policy

CPL3 may invoke only:

| ID | Service |
|---:|---|
| 2 | `SYS_GET_TICKS` |
| 3 | `SYS_GET_HEALTH` |
| 8 | `SYS_SYSTEM_INFO` |
| 9 | `SYS_USER_EXIT` |

All arguments are bounded scalars. No raw pointer ABI exists. `SYS_USER_EXIT`
is denied to CPL0, and all non-allowlisted calls are denied to CPL3.

## Fault Evidence

`user-fault-test confirm` enters the fixed CPL3 fault routine and reads a
supervisor-only kernel address. Vector 14 records CR2, error code, EIP, CS,
EFLAGS, caller CPL, user ESP, and user SS. The kernel does not recover; it
exports `[USER_FAULT]` evidence and halts. Run this only as the final command in
a disposable QEMU session attached to a copied recovery image.

The analyzer treats a fault as an expected test only when the kernel report
explicitly says `fault_origin=cpl3` and `intentional_test=yes`. An unexpected
CPL3 fault remains critical and causes a nonzero analyzer exit.

## Commands And Reports

```text
ring3-readiness
user-memory
ring3-test
ring3
user-fault-test confirm
```

Reports: `[RING3]`, `[USER_MEMORY]`, `[USER_EXECUTION]`, and `[USER_FAULT]`.

## Boundary

Real: CPL3 execution, minimal user PTEs, TSS ESP0 entry, origin-checked
syscalls, controlled kernel return, and CPL3 fault frames.

Not implemented: ELF loading, arbitrary programs, TinyFS execution, multiple
processes, per-process page tables, process isolation, scheduler-driven
userspace, a stable ABI, user/kernel copy helpers, demand paging, stack growth,
guard pages, or untrusted code execution.
