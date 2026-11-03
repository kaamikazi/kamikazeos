# Changelog

## v2.6.0 - Ring 3 Seed (development)

- Verified one explicitly approved disposable-QEMU `iretd` transition to the
  fixed linked CPL3 routine and return through `SYS_USER_EXIT`.
- Added a read-only user code page and fixed four-page, 16 KiB user stack.
- Kept the kernel, heap, page structures, GDT/IDT/TSS, ATA buffers, and ESP0
  stack supervisor-only through exact PTE validation and USER-page counting.
- Added CPL3 caller tracking and a four-call CPL3 syscall allowlist.
- Added the held CPL3-only `SYS_USER_EXIT` return path and its validation.
- Extended exception frames with saved CPL, user ESP, and user SS evidence.
- Added `ring3`, `ring3-readiness`, `ring3-test`, `user-memory`, and guarded
  `user-fault-test confirm` commands.
- Added `[RING3]`, `[USER_MEMORY]`, `[USER_EXECUTION]`, and `[USER_FAULT]`
  reports plus analyzer and read-only dashboard support.
- Hardened semantic GDT validation, common exception frames, syscall caller
  origin checks, register restoration tests, and Multiboot map normalization.
- Added single-use approval consumption, an independent disabled fault-test
  gate, and user-side GPR, segment, stack, and IF preservation checks. The
  default build is restored to `execution_approved=no` after verification.
- Added no ELF loading, TinyFS execution, arbitrary user binaries, multiple
  processes, networking, downloads, automatic repair, or self-modification.

## v2.5.0 - Privilege Boundary

- Added flat DPL3 user code and data descriptors with explicit selectors.
- Added a loaded 32-bit TSS with SS0 and a bounded 16 KiB ESP0 kernel stack.
- Added privilege readiness validation and DPL0 fallback for the syscall gate.
- Made `int 0x80` DPL3-capable only after GDT, TSS, ESP0, and paging validate.
- Preserved segment registers correctly across the syscall assembly path.
- Added `gdt`, `tss`, `privilege`, and `ring3-readiness` shell commands.
- Added `[GDT]`, `[TSS]`, `[PRIVILEGE]`, and `[RING3_READINESS]` reports.
- Updated the analyzer, JSON exporter, and read-only Recovery Center.
- Added no Ring 3 execution, user stack/pages, ELF loading, process isolation,
  per-process address spaces, arbitrary execution, networking, or auto-repair.

## v2.4.0 - Syscall Door

- Added a DPL0 `int 0x80` IDT gate with a normalized assembly entry stub.
- Added a fixed compile-time registry of eight kernel service calls.
- Added a dispatcher with bounded scalar/handle validation and no pointer ABI.
- Added safe result codes, unknown-ID rejection, call/failure statistics, and
  structured error evidence.
- Added `syscalls`, `syscall-test`, and `syscall <id>` shell commands.
- Added `[SYSCALLS]` and `[SYSCALL_TABLE]` serial report sections.
- Updated the offline analyzer, JSON exporter, and read-only Recovery Center.
- Added no Ring 3 execution, userspace pointers, ELF loading, process
  isolation, arbitrary execution, networking, downloads, or automatic repair.

## v2.3.0 - Paging Gate

- Added 32-bit x86 paging structures for a low-memory identity map.
- Added strict validation for the GDT, early exception IDT, page-directory and
  page-table alignment/flags, unused PDEs, and required runtime ranges.
- Added CR3 loading and the gated CR0.PG enable path.
- Added `paging`, `page-map`, and guarded `pagefault-test confirm` commands.
- Added `[PAGING]`, `[PAGE_MAP]`, and `[PAGE_FAULT]` serial report sections.
- Added page fault CR2 evidence recording through the existing exception path.
- Updated analyzer and Recovery Center with read-only paging visibility.
- Paging is enabled with a supervisor-writable 0-16 MiB identity map after the
  validation gate passes; validation failure leaves paging disabled.
- Added no userspace virtual memory, per-process address spaces, demand paging,
  copy-on-write, memory protection between tasks, networking, auto-repair,
  arbitrary execution, or self-modification.

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
