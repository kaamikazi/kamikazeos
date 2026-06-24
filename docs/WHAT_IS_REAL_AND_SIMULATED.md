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
- COM1 serial logs and boot reports.
- Structured kernel error log.
- GhostAttach policy decisions over fixed device/driver registries.
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

## Why Simulate These Parts

The project is about building a safe recovery/update control plane before
adding any dangerous data plane. Simulated package application allows the
approval, audit, rollback, reporting, and dashboard flows to be tested without
creating a path for arbitrary code execution.
