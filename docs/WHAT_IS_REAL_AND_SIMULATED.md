# What Is Real And Simulated

KamikazeOS is intentionally honest about boundaries. The project demonstrates
real OS and tooling mechanics, but avoids dangerous update behavior.

## Real

- QEMU-bootable 32-bit x86 kernel.
- VGA text output.
- PS/2 keyboard polling shell.
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
- Offline analyzer and static Recovery Center dashboard.

## Simulated

- In-kernel Ed25519 cryptographic verification.
- Binary patching.
- Actual module replacement.
- Driver downloads.
- Internet update fetching.
- AI auto-repair.
- Kernel self-modification.
- Arbitrary package loading or execution.

## Why Simulate These Parts

The project is about building a safe recovery/update control plane before
adding any dangerous data plane. Simulated package application allows the
approval, audit, rollback, reporting, and dashboard flows to be tested without
creating a path for arbitrary code execution.
