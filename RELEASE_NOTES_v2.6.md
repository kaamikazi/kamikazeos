# KamikazeOS v2.6.0 "Ring 3 Seed"

v2.6 is the first KamikazeOS release to execute a controlled routine at x86
CPL3 and return safely to the original Ring 0 shell context.

## Highlights

- Builds an `iretd` frame with fixed DPL3 selectors, linked EIP, bounded ESP,
  and deliberate `EFLAGS=0x202`.
- Maps one user/read-only code page and four user/writable stack pages while
  validating that all other low identity mappings remain supervisor-only.
- Exercises the existing DPL3 `int 0x80` gate and observes the TSS ESP0 stack
  transition.
- Adds CPL3-only `SYS_USER_EXIT`, caller-origin enforcement, and controlled
  restoration of the original kernel stack.
- Captures CPL3 exception frames and CR2 page-fault evidence before halting.
- Adds shell commands, serial report sections, analyzer interpretation, and
  read-only Recovery Center panels for the privilege boundary.

## Safety Boundary

The only executable CPL3 code is a fixed routine linked at build time. v2.6
does not load ELF files, execute TinyFS content, accept user machine code,
create general processes, or expose caller pointers. It adds no networking,
downloads, automatic repair, binary patching, or kernel self-modification.

The 0-16 MiB identity map is supervisor-only by default. Exactly five expected
PTEs are user-accessible. Ring 3 execution fails closed unless code, stack,
kernel, page-structure, and total USER-page checks all pass.

## Verification Boundary

Normal boot and round-trip tests are non-destructive when run against a copied
recovery image. `user-fault-test confirm` intentionally halts the VM and must
be run last in a disposable session. It does not attempt fault recovery.

## Still Not Implemented

- ELF or arbitrary executable loading
- multiple processes or scheduler-driven userspace
- process isolation or per-process address spaces
- stable userspace ABI or user/kernel copy helpers
- demand paging, user-stack growth, or guard pages
- execution from TinyFS or package payloads
