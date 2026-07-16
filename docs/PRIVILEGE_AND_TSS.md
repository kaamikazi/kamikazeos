# Privilege And TSS

KamikazeOS v2.5 "Privilege Boundary" established the x86 structures required
for a controlled privilege transition. v2.6 uses them for one fixed linked
CPL3 test routine.

## GDT Layout

| Selector | Descriptor | DPL |
|---:|---|---:|
| `0x00` | Null | - |
| `0x08` | Flat kernel code | 0 |
| `0x10` | Flat kernel data | 0 |
| `0x1B` | Flat user code, including RPL3 | 3 |
| `0x23` | Flat user data, including RPL3 | 3 |
| `0x28` | Available 32-bit TSS | 0 |

The user descriptors cover the flat 32-bit address space, but paging entries
remain supervisor-only. A segment descriptor alone does not grant access to a
page from Ring 3.

## TSS And ESP0

The kernel owns one packed 104-byte 32-bit TSS. `ss0` is `0x10`, and `esp0`
points to the 16-byte-aligned top of a dedicated 16 KiB static kernel stack.
The stack bounds are validated before ESP0 is accepted. `ltr 0x28` loads the
task register, and `str` verifies the selected descriptor.

The I/O-map offset is set to the end of the TSS, so no I/O permission bitmap is
exposed. The TSS exists only to provide a future privilege-transition kernel
stack. KamikazeOS does not use hardware TSS task switching.

The current guard policy is `bounds_checked_no_guard_page`. Paging explicitly
validates the entire transition stack, but no unmapped guard page exists yet.

## Syscall Gate Policy

The `int 0x80` gate becomes DPL3 only when all of these checks pass:

- kernel GDT loaded
- user code/data descriptors valid
- TSS loaded and task-register selector verified
- SS0 and ESP0 transition-stack bounds valid
- identity paging enabled and validated

If any check fails, the gate stays DPL0 and reports
`syscall_user_gate=disabled`. The assembly stub preserves general and segment
registers and switches to kernel data selectors before calling C code.

No syscall accepts raw pointers. General syscall tests originate in Ring 0;
the fixed CPL3 routine is limited to an explicit scalar-only allowlist.

## Readiness Meaning

`privilege_ready_for_ring3()` means the descriptors, TSS, ESP0 stack, paging,
and syscall gate are internally consistent. v2.6 separately validates the
fixed user code and stack mappings before reporting `controlled_test_ready`.

Not implemented:

- ELF or executable loading
- arbitrary or disk-supplied user code
- multiple processes or scheduler-driven userspace tasks
- process isolation or per-process address spaces
- per-process permissions or stable userspace ABI
- user/kernel copy helpers
- TSS hardware task switching
- paging guard pages

## Inspection

```text
gdt
tss
privilege
ring3-readiness
user-memory
ring3-test
ring3
user-fault-test confirm
syscalls
syscall-test
```

The serial report exports `[GDT]`, `[TSS]`, `[PRIVILEGE]`, and
`[RING3_READINESS]`. The analyzer and Recovery Center consume these sections
read-only and never change kernel state.
