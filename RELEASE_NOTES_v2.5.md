# KamikazeOS v2.5.0 "Privilege Boundary"

v2.5 adds the architectural privilege foundation for a possible future
controlled Ring 3 transition. It extends the kernel-owned GDT with DPL3 code
and data descriptors, loads a 32-bit TSS, configures a bounded ESP0 kernel
stack, and gates DPL3 access to `int 0x80` on explicit readiness checks.

## Added

- Explicit kernel, user, and TSS selector constants.
- Validated flat DPL3 code and data descriptors.
- One loaded 32-bit TSS with SS0 and bounded ESP0 configuration.
- Dedicated 16 KiB privilege-transition kernel stack.
- Privilege readiness checks with DPL0 syscall-gate fallback.
- Segment-safe syscall assembly entry/return handling.
- `gdt`, `tss`, `privilege`, and `ring3-readiness` shell commands.
- `[GDT]`, `[TSS]`, `[PRIVILEGE]`, and `[RING3_READINESS]` reports.
- Analyzer, JSON exporter, and read-only Recovery Center visibility.

## Safety Boundary

The DPL3 gate is real, but all current callers remain Ring 0. v2.5 does not add
Ring 3 execution, a user stack, user-accessible page mappings, ELF loading,
process isolation, per-process address spaces, arbitrary code execution,
networking, downloads, automatic repair, binary patching, or kernel
self-modification.

`foundation_ready` is an architectural readiness result, not a claim that
userspace execution is enabled. The TSS is not used for hardware task
switching. The ESP0 stack is bounded and mapped, but has no guard page.

## Compatibility

The storage layout and on-disk recovery formats are unchanged. Existing v2.4
RecoveryStore, package, slot, and TinyFS images remain compatible. The shell,
syscall registry, package policy, recovery architecture, analyzer safety, and
read-only dashboard constraints are preserved.
