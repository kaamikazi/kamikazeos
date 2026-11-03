# Prepared Ring 3 Boundary

KamikazeOS v2.6 "Ring 3 Seed" contains a fixed CPL0-to-CPL3 transition design.
The target is a trusted routine linked into the kernel image at build time.
One explicitly approved disposable-QEMU run verified the hardened round trip.
Approval was consumed before entry, and the default audited build is restored
to `execution_approved=no`. Nothing is loaded or executed from TinyFS,
packages, or host input.

## Transition Design

The `ring3_enter_user()` path saves the original Ring 0 flags and registers, records the
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

The test routine calls `SYS_GET_TICKS`, verifies preserved GPRs, segment
selectors, stack position, and IF, then calls `SYS_USER_EXIT`. On `int 0x80`,
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

`user-fault-test confirm` is independently gated and remains disabled. After
a separate destructive-test approval, it is designed to enter the fixed CPL3
fault routine and read a
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
ring3
```

`ring3-test` currently refuses execution because the approved one-shot token
was consumed and the release gate was restored. `user-fault-test confirm`
refuses through a separate gate. Both remain omitted from the normal demo.

Reports: `[RING3]`, `[USER_MEMORY]`, `[USER_EXECUTION]`, and `[USER_FAULT]`.

## Boundary

Real and verified for one fixed routine: minimal user PTEs, `iretd` entry,
loaded TSS/ESP0 stack switching, full privilege-transition frames,
origin-checked CPL3 syscalls, register/segment/stack preservation checks,
`SYS_USER_EXIT`, and restoration of the original Ring 0 shell context.

Prepared but not post-hardening tested: CPL3 fault capture. The independent
fault-test gate remains disabled.

Not implemented: ELF loading, arbitrary programs, TinyFS execution, multiple
processes, per-process page tables, process isolation, scheduler-driven
userspace, a stable ABI, user/kernel copy helpers, demand paging, stack growth,
guard pages, or untrusted code execution.
