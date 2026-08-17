# KamikazeOS v2.6.0 "Ring 3 Seed" (Draft)

The v2.6 branch prepares a controlled routine for x86 CPL3. Following the
v2.5 adversarial audit, `execution_approved=no` keeps the transition disabled.
These notes are draft release metadata, not a claim that the hardened branch
has executed or returned from CPL3.

## Highlights

- Builds an `iretd` frame with fixed DPL3 selectors, linked EIP, bounded ESP,
  and deliberate `EFLAGS=0x202`.
- Maps one user/read-only code page and four user/writable stack pages while
  validating that all other low identity mappings remain supervisor-only.
- Prepares the existing DPL3 `int 0x80` gate and TSS ESP0 stack transition.
- Prepares CPL3-only `SYS_USER_EXIT`, caller-origin enforcement, and controlled
  restoration of the original kernel stack.
- Prepares CPL3 exception-frame capture; Ring 0 CR2 page-fault evidence remains
  verified before halt.
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

Normal Ring 0 tests use copied recovery images. `ring3-test` and
`user-fault-test confirm` are refused by the current audit hold. A future
approved transition test must use a disposable image and run fault testing
last; no fault recovery is planned.

## Still Not Implemented

- ELF or arbitrary executable loading
- multiple processes or scheduler-driven userspace
- process isolation or per-process address spaces
- stable userspace ABI or user/kernel copy helpers
- demand paging, user-stack growth, or guard pages
- execution from TinyFS or package payloads
