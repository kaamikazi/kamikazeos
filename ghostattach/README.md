# GhostAttach Driver Layer — arrives in Phase 3

This directory will hold the deterministic driver-attach policy engine:
device enumeration (PS/2 probe, PCI config-space scan), the verified driver
registry, the attach pipeline (`exact match → class fallback → isolate`),
and boot-loop prevention.

Design rules (from [docs/00-architecture.md](../docs/00-architecture.md)):
- **No AI in this layer.** GhostAttach is a policy machine the AI may consult,
  never bypass.
- Never auto-load an unverified driver. Unknown devices are reported, not guessed at.
- Every attach attempt — success or failure — produces a structured klog entry.

In Phase 1, a preview lives in the kernel's `devices` shell command.
