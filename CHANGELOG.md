# Changelog

## v2.2.0 - Heap Seed

- Added a fixed-region kernel bump heap initialized from the memory manager.
- Added 16-byte aligned `kheap_alloc()` with zero-size and oversize rejection.
- Added heap usage, remaining, allocation, failure, and high-water stats.
- Added `heap`, `heap-test`, and `heap-alloc <bytes>` shell commands.
- Added `[KERNEL_HEAP]` serial report section.
- Updated analyzer and Recovery Center with read-only heap visibility.
- Added no `free()`, heap growth, virtual memory, userspace heap,
  malloc-compatible allocator, networking, auto-repair, arbitrary execution,
  or self-modification.

## v2.1.0 - Memory Map

- Added Multiboot v1 memory map parsing with a conservative fallback.
- Added fixed 4 KiB physical page tracking for the demo memory range.
- Reserved low memory and the linked kernel image in the page tracker.
- Added `mem`, `memmap`, and `pages` shell commands.
- Added `[MEMORY]`, `[MEMORY_MAP]`, and `[PAGES]` serial report sections.
- Updated analyzer and Recovery Center with read-only memory visibility.
- Added no virtual memory, heap allocator, userspace memory, process address
  spaces, networking, auto-repair, arbitrary execution, or self-modification.

## v2.0.0 - Real Kernel Demo

- Added `system` shell command for integrated readiness status.
- Added `diagnose` shell command for compact full-system diagnostics.
- Added `[SYSTEM_STATUS]` and `[DIAGNOSTICS_SUMMARY]` serial report sections.
- Updated analyzer with a v2.0 integrated system summary.
- Added Recovery Center v2.0 overview panel.
- Added v2.0 demo script and release/demo documentation.
- Added no networking, auto-repair, binary patching, downloads, real Ring 3
  userspace, virtual memory, arbitrary execution, or kernel self-modification.

## v1.9.0 - Userland Spark

- Added a fixed kernel-managed app registry.
- Added an app policy gate that allows only compile-time built-in apps.
- Added `apps`, `app <name>`, `run <name>`, and `app-log` shell commands.
- Added safe built-in apps: `hello`, `sysinfo`, `tinyfs-demo`,
  `recovery-summary`, `task-demo`, and `health-report`.
- Added TinyFS-backed demo app behavior through bounded TinyFS APIs.
- Added `[APPS]` and `[APP_LOG]` serial report sections.
- Added analyzer and read-only Recovery Center app visibility.
- Added no Ring 3 userspace, process isolation, executable loading, ELF loader,
  per-process memory, syscalls, virtual memory, or arbitrary code execution.

## v1.8.0 - TinyFS

- Added a checksummed TinyFS superblock at LBA 64.
- Added a serialized fixed file table at LBA 65-72.
- Added bounded first-fit file data allocation at LBA 73-127.
- Added a 16-file limit, 24-character validated names, and 2048-byte files.
- Added checksum-verified text create, write, read, list, and delete operations.
- Added `fs`, confirmed `fs-format`, `ls`, `touch`, `write`, `cat`, and `rm`.
- Added TinyFS serial reporting, analyzer support, and read-only dashboard views.
- Added no directories, permissions, dynamic allocation, filesystem journal,
  crash-safe transactions, userspace, arbitrary execution, or self-modification.

## v1.7.0 - Task Seed

- Added a statically allocated kernel task table with six explicit states.
- Added cooperative `task_yield()` step dispatch without stack switching.
- Added counter-only idle, health-monitor, and recovery-watch demo tasks.
- Added PIT IRQ tick accounting for the current logical task.
- Added `tasks`, `task-current`, `task-yield`, and `task-demo` shell commands.
- Added `[KERNEL_TASKS]` serial reporting and offline analyzer support.
- Added read-only Recovery Center task status and table views.
- Added no userspace, process isolation, preemptive switching, per-task stacks,
  virtual memory, networking, downloads, arbitrary execution, or self-modification.

## v1.6.0 - Kernel Discipline

- Added a 256-entry x86 IDT and `lidt` initialization.
- Added normalized assembly stubs for CPU exceptions, PIT IRQ0, and keyboard
  IRQ1.
- Added non-returning exception diagnostics with vector, error code,
  instruction pointer, structured logging, serial evidence, and panic state.
- Remapped the legacy 8259 PIC to vectors 32-47 with masking and EOI support.
- Added a 100 Hz PIT tick counter and `ticks` command.
- Added IRQ-driven PS/2 keyboard input with the original polling path retained
  as an explicit fallback.
- Added `interrupts` and confirmed `exceptions-test` shell commands.
- Added interrupt, timing, and exception serial report sections.
- Updated the offline analyzer and read-only Recovery Center for kernel
  interrupt evidence.
- Added no networking, auto-repair, binary patching, downloads, arbitrary code
  execution, or kernel self-modification.

## v1.5.0 - Public Demo Release

- Updated public release docs for the stable demo package.
- Added one-command demo preparation scripts.
- Added dashboard generation helper script.
- Added public demo script, demo flow, and portfolio summary artifacts.
- Updated honest real-vs-simulated documentation.
- No networking, package downloads, auto-repair, binary patching, driver
  downloads, arbitrary code execution, or kernel self-modification added.

## v1.4.0 - Recovery Center Pro

- Rebuilt the Recovery Center into a portfolio-grade read-only dashboard.
- Added system status, health, storage, trust/signature, package, journal,
  snapshot, and command-copy panels.
- Added dashboard docs and demo guidance.

## v1.3.0 - Public Release

- Reworked README for public demo and portfolio use.
- Added architecture, command, safety, recovery flow, demo, troubleshooting,
  screenshot, portfolio, and release documentation.
- Added a PowerShell demo helper.
- Updated public release version metadata.
- Clarified `.gitignore` coverage for `build/recovery.img`, Python cache
  files, build artifacts, and private keys.
- No new dangerous functionality added.

## v1.2.0 - Trust Forge

- Added host-side Ed25519 key generation, signing, and signature verification.
- Added deterministic signed payloads.
- Added signed manifest fields.
- Added import refusal for unsigned or tampered manifests.
- Updated kernel reports, shell, analyzer, dashboard, and docs for signature
  and trust fields.

## v1.1.0 - Offline Package Manifest Tooling

- Added manifest create/validate/import/export host tools.
- Added example manifests.
- Added kernel visibility for imported disk manifests.
- Updated analyzer and Recovery Center package views.

## v1.0.0

- Added disk-backed package manifests, persistent package state, A/B module
  slot metadata, trust-anchor simulation, pending verification tracking,
  analyzer support, and dashboard support.
