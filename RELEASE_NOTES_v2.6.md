# KamikazeOS v2.6.0 "Ring 3 Seed" (Draft)

The v2.6 branch contains a controlled routine for x86 CPL3. Following the
pre-execution hardening audit, one explicitly approved disposable-QEMU run
entered CPL3, invoked the origin-checked syscall gate on the TSS ESP0 stack,
and returned to the original Ring 0 shell through `SYS_USER_EXIT`. The
single-use approval was consumed; the default build remains held.

## Highlights

- Builds an `iretd` frame with fixed DPL3 selectors, linked EIP, bounded ESP,
  and deliberate `EFLAGS=0x202`.
- Maps one user/read-only code page and four user/writable stack pages while
  validating that all other low identity mappings remain supervisor-only.
- Verifies the DPL3 `int 0x80` gate and TSS ESP0 stack transition.
- Verifies CPL3-only `SYS_USER_EXIT`, caller-origin enforcement, register and
  segment preservation, and restoration of the original kernel stack.
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

Normal Ring 0 tests use copied recovery images. The approved `ring3-test`
completed once, reported `CS=0x1B`, data selectors `0x23`, two validated CPL3
syscalls, an ESP0 switch, context preservation, and continued shell activity.
The release posture again refuses `ring3-test`. `user-fault-test confirm`
remained independently disabled and was not run after the hardening audit.

## Still Not Implemented

- ELF or arbitrary executable loading
- multiple processes or scheduler-driven userspace
- process isolation or per-process address spaces
- stable userspace ABI or user/kernel copy helpers
- demand paging, user-stack growth, or guard pages
- execution from TinyFS or package payloads
