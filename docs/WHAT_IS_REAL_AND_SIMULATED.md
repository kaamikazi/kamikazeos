# What Is Real And Simulated

KamikazeOS is intentionally honest about boundaries. The project demonstrates
real OS and tooling mechanics, but avoids dangerous update behavior.

## Real

- QEMU-bootable 32-bit x86 kernel.
- VGA text output.
- A 256-entry x86 Interrupt Descriptor Table loaded with `lidt`.
- CPU exception stubs and non-returning diagnostic handlers.
- Legacy 8259 PIC remapping and IRQ masking/EOI handling.
- PIT channel 0 timer interrupts and a live 100 Hz tick counter.
- PS/2 keyboard IRQ1 input with a fixed ring buffer.
- PS/2 keyboard polling fallback if IRQ mode is unavailable.
- Fixed-size kernel task records and task states.
- Cooperative dispatch of fixed kernel-owned step functions.
- PIT tick accounting against the current logical task.
- Task health data in serial reports, analyzer, and dashboard.
- Fixed-sector TinyFS superblock and file table on LBA 64-127.
- Bounded text file creation, first-fit writes, checksummed reads, and deletion.
- TinyFS shell commands and read-only report/dashboard visibility.
- Fixed kernel-managed app registry.
- Policy-gated `run <app>` command for compile-time built-in apps.
- TinyFS-backed demo app using bounded filesystem APIs.
- App run/block history in serial reports, analyzer, and dashboard.
- Integrated v2.0 `system` and `diagnose` summaries.
- v2.0 serial report overview sections for system status and diagnostics.
- Multiboot memory map parsing when the bootloader provides it.
- Conservative memory-map fallback when Multiboot data is unavailable.
- Fixed 4 KiB physical page tracking with low-memory and kernel reservations.
- Memory status, memory map, and page reports in shell, analyzer, and dashboard.
- Bounded fixed-region kernel bump heap.
- Heap allocation bounds checks, stats, serial report, analyzer, and dashboard.
- Prepared 32-bit x86 paging structures.
- Identity page-map validation and paging reports.
- Page fault CR2 evidence path when a page fault occurs.
- COM1 serial logs and boot reports.
- Structured kernel error log.
- GhostAttach policy decisions over fixed device/driver registries.
- GhostAttach's virtual failure canary is reported separately from production
  attach failures and does not reduce the production health score.
- CMOS recovery memory fallback.
- ATA PIO access to a raw recovery disk image.
- Durable RecoveryStore journal.
- Boot snapshots and deterministic health scoring.
- Fixed RecoveryEngine action catalog and policy gates.
- Disk-backed package manifests.
- Persistent package lifecycle state.
- A/B module slot metadata.
- Host-side manifest validation.
- Host-side deterministic checksums.
- Real host-side Ed25519 manifest signing and verification.
- Kernel-side checksum, trust-anchor, signature metadata, and policy checks.
- Offline analyzer and Recovery Center Pro dashboard.

## Simulated

- In-kernel Ed25519 cryptographic verification.
- Binary patching.
- Actual module replacement.
- Driver downloads.
- Internet update fetching.
- AI auto-repair.
- Kernel self-modification.
- Arbitrary package loading or execution.
- Ring 3 userspace.
- Process isolation.
- Executable loading or ELF loading.
- Per-process memory and syscalls.
- Enabled virtual memory.
- Paging isolation.
- Per-process page tables.
- Demand paging.
- Copy-on-write.
- General-purpose `malloc/free` heap allocation.
- `free()` in the kernel bump allocator.
- Heap growth.
- Userspace heap allocation.
- Process address spaces.

## v1.6 Kernel Boundary

The v1.6 kernel has real interrupt gates, exception entry stubs, PIC remapping,
timer ticks, and IRQ keyboard input. Exception recovery is not simulated: it is
explicitly absent. The kernel records evidence and halts safely.

The project still has no userspace, process isolation, preemptive context
switching, per-task stacks, virtual memory, real process model, syscalls,
networking, real binary patching, actual module replacement, AI-driven repair,
or in-kernel Ed25519 math.

## v1.7 Task Boundary

The task table, task states, explicit cooperative yields, built-in step
functions, and tick accounting are real. The shell and task steps still execute
on one kernel stack in ring 0. A task record is not a userspace process, and
the PIT never switches execution contexts.

## v1.8 Filesystem Boundary

TinyFS performs real ATA sector reads and durable writes inside its fixed LBA
range. Its metadata, allocation checks, filename checks, size limits, and
content checksums are real. It has no directories, permissions, userspace
files, dynamic allocation, journal, transactions, or crash-safe update
protocol. File contents are never loaded or executed.

## v1.9 App/Userland Boundary

The app registry, app policy gate, `run <app>` command, built-in app functions,
TinyFS-backed demo app, and app reports are real. The shell can run only
compile-time registered functions. Unknown app names are blocked and logged.

This is not real userspace. Apps still execute in kernel context on the kernel
stack. There is no Ring 3 transition, process isolation, ELF loader, syscall
ABI, virtual memory, or arbitrary executable loading.

## v2.0 Integrated Demo Boundary

v2.0 is a stabilization and integration milestone. The new `system` and
`diagnose` commands summarize existing real subsystems; they do not add
automatic repair or new execution privileges. Analyzer and dashboard output
remain read-only and may recommend or copy manual shell commands only.

## v2.1 Memory Boundary

v2.1 parses real bootloader memory-map data when present and maintains a fixed
physical page tracker. The page tracker is accounting only: there is no virtual
memory, no paging isolation, no heap allocator, no userspace memory model, and
no process address space management.

## v2.2 Heap Boundary

v2.2 adds a real bounded kernel bump heap. Allocations are checked, aligned, and
kept inside the fixed heap range. The allocator does not support `free()`, heap
growth, malloc-compatible semantics, virtual memory, userspace heaps, or
per-process address spaces.

## v2.3 Paging Boundary

v2.3 prepares and validates paging data structures, but does not enable paging.
The kernel still runs with paging disabled. There are no userspace address
spaces, per-process page tables, demand paging, copy-on-write, or memory
protection between tasks.

## Package Transaction Boundary

Package lifecycle hardening uses staged metadata, manifest identity binding,
and mirrored LBA 48-49 state copies to reconcile interrupted metadata writes.
This is real bounded update-state recovery, not a claim of general filesystem
transactions, atomic hardware writes, or actual module replacement.

## Why Simulate These Parts

The project is about building a safe recovery/update control plane before
adding any dangerous data plane. Simulated package application allows the
approval, audit, rollback, reporting, and dashboard flows to be tested without
creating a path for arbitrary code execution.
