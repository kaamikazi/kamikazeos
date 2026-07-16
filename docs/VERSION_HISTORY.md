# KamikazeOS Version History

## v2.4.0 "Syscall Door"

Added a dedicated DPL0 `int 0x80` gate, normalized assembly stub, fixed
eight-entry syscall registry, validated dispatcher, result codes, counters,
shell commands, serial reports, analyzer interpretation, and read-only
Recovery Center visibility. Arguments are bounded scalar values and registry
indexes; text results use kernel-owned buffers. No Ring 3 callers, userspace
pointers, process isolation, ELF loading, arbitrary execution, networking,
downloads, automatic repair, or ABI stability guarantee was added.

## v2.3.0 "Paging Gate"

Added the first safe paging foundation: 32-bit x86 page directory/table
structures, a validated supervisor identity map, CR3 loading, gated CR0.PG
enablement,
`paging`, `page-map`, and guarded `pagefault-test confirm` shell commands,
`[PAGING]`, `[PAGE_MAP]`, and `[PAGE_FAULT]` serial report sections, page-fault evidence
recording, analyzer support, and read-only Recovery Center paging visibility.
Paging is enabled only after all required kernel runtime ranges validate. No
userspace virtual memory, per-process address spaces, demand paging,
copy-on-write, memory
protection between tasks, networking, auto-repair, arbitrary execution, or
self-modification was added.

## v2.2.0 "Heap Seed"

Added the first bounded kernel heap allocator foundation: a fixed-size bump
allocator region supplied by the memory manager, 16-byte aligned allocations,
oversize/zero-size rejection, allocation/failure stats, `heap`, `heap-test`,
and `heap-alloc <bytes>` shell commands, `[KERNEL_HEAP]` serial reporting,
analyzer support, and read-only Recovery Center heap visibility. No `free()`,
heap growth, virtual memory, userspace heap, malloc-compatible API, process
address spaces, networking, auto-repair, arbitrary execution, or
self-modification was added.

## v2.1.0 "Memory Map"

Added the first kernel memory management foundation: Multiboot memory map
parsing when available, a conservative fallback map, fixed 4 KiB physical page
tracking, low-memory and kernel image reservation, `mem`, `memmap`, and
`pages` shell commands, serial `[MEMORY]`, `[MEMORY_MAP]`, and `[PAGES]`
sections, analyzer support, and read-only Recovery Center memory views. No
virtual memory, paging isolation, heap allocator, userspace memory, process
address spaces, networking, auto-repair, arbitrary execution, or
self-modification was added.

## v2.0.0 "Real Kernel Demo"

Integrated milestone release. Added `system` and `diagnose` shell commands,
`[SYSTEM_STATUS]` and `[DIAGNOSTICS_SUMMARY]` report sections, v2.0 analyzer
summary output, Recovery Center overview panel, demo script, and release docs.
No networking, auto-repair, binary patching, downloads, real Ring 3 userspace,
virtual memory, arbitrary code execution, or kernel self-modification was
added.

## v1.9.0 "Userland Spark"

Added the first userland-style app model using a fixed compile-time registry,
a policy-gated `run <app>` shell command, built-in diagnostic/demo apps,
TinyFS-backed `tinyfs-demo`, app run history, serial app reports, analyzer
support, and read-only Recovery Center views. This is not Ring 3 userspace:
there is no process isolation, executable loading, ELF loader, per-process
memory, syscall ABI, virtual memory, or arbitrary code execution.

## v1.8.0 "TinyFS"

Added a fixed-sector file abstraction on LBA 64-127 with a checksummed
superblock, serialized 16-entry file table, bounded first-fit data allocation,
text create/read/write/delete commands, serial reporting, analyzer support,
and read-only Recovery Center views. No directories, permissions, dynamic
allocation, journaling, crash-safe transactions, userspace, or file execution
were added.

## v1.7.0 "Task Seed"

Added a fixed-size kernel task table, cooperative step scheduler skeleton,
three counter-only demo tasks, PIT tick accounting, task shell commands, serial
task health reporting, analyzer support, and read-only Recovery Center task
views. No userspace, per-task stacks, process isolation, virtual memory, or
preemptive context switching was added.

## v1.6.0 "Kernel Discipline"

Added the kernel's first real interrupt foundation: a loaded x86 IDT, assembly
exception/IRQ stubs, CPU exception diagnostics, legacy PIC remapping, PIT timer
ticks, and PS/2 keyboard IRQ input with a polling fallback. Added shell/report,
offline analyzer, and read-only Recovery Center visibility. CPU exceptions are
recorded and halt safely; no automatic exception recovery was added.

## v1.5.0 "Public Demo Release"

Stable public demo package. Added demo helper scripts, dashboard generation
workflow, release notes, demo scripts, and portfolio-ready summaries. No new
dangerous OS functionality was added.

## v1.4.0 "Recovery Center Pro"

Polished the Recovery Center into a portfolio-grade read-only dashboard with
status cards, health trend, storage/trust panels, timelines, package tables,
and copy-only command chips.

## v1.3.0 "Public Release"

Documentation, demo, and release polish. No new dangerous functionality was
added. v1.2 remains the latest technical feature milestone.

## v1.2.0 "Trust Forge"

Added offline signing tools:

- `generate_keypair.py`
- `sign_manifest.py`
- `verify_signature.py`

Host-side Ed25519 signing and verification are real. Kernel-side Ed25519
verification remains simulated, while checksum, trust-anchor identity,
signature metadata, and policy gates are enforced by the kernel.

## v1.1.0 "Offline Package Manifest Tooling"

Added host tools to create, validate, import, and export package manifests
into/from `build/recovery.img`. Imported manifests appear in the shell,
serial reports, analyzer, and Recovery Center.

## v1.0.0

Added disk-backed manifests, persistent package state, A/B module slot
metadata, trust-anchor simulation, pending verification tracking, analyzer
support, and Recovery Center support.

## v0.9.0 "Controlled Burn"

Added package lifecycle commands and policy gates:

- `approve-pkg`
- `apply-pkg`
- `verify-pkg`
- `rollback-pkg`

Application was simulated as module version metadata only.

## v0.8.0 "Vital Signs"

Added boot snapshots, health scoring, and recovery timeline reporting.

## v0.7.0 "Black Box"

Added ATA PIO disk-backed RecoveryStore and durable journal.

## v0.6.0 "Long Memory"

Added RecoveryMemory using CMOS/NVRAM with disk fallback added later.

## v0.5.0 "Closed Loop"

Added RecoveryEngine fixed action catalog, policy gates, approvals, and
rollbacks.

## v0.4.0 "Open Channel"

Added serial boot reports and the offline analyzer.

## v0.3.0 "Ghost in the Machine"

Added GhostAttach device/driver policy, structured logs, and shell commands.
