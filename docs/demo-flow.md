# KamikazeOS 5-Minute Technical Walkthrough

## 1. Boot And Shell

Build and boot:

```powershell
.\scripts\build.ps1
.\scripts\run-demo.ps1
```

In QEMU:

```text
about
help
```

Explain: 32-bit x86 kernel, VGA text output, PS/2 keyboard polling, COM1
serial reporting.

## 2. Recovery State

Run:

```text
health
store
journal
snapshots
timeline
```

Explain: RecoveryStore is a raw ATA PIO disk-backed state store; the journal is
durable; snapshots produce a deterministic health score.

## 3. GhostAttach

Run:

```text
devices
drivers
ghostattach
```

Explain: GhostAttach uses a fixed device/driver registry and policy-gated
fallbacks. Unverified drivers are blocked.

## 4. Package Trust

Run:

```text
trust
manifests
packages
package PKG-101
```

Explain: host-side Ed25519 signing is real; kernel-side Ed25519 math is
simulated; the kernel still enforces checksums, trust anchor identity, signature
metadata, payload type, risk, and reversibility.

## 5. Lifecycle

Run:

```text
approve-pkg PKG-101
apply-pkg PKG-101
slots
verify-pkg PKG-101
package PKG-101
```

Explain: package application is metadata-only. It changes module version/A-B
slot metadata, not binaries.

## 6. Analyzer And Dashboard

Run:

```text
export-logs
```

On the host:

```powershell
.\scripts\generate-dashboard.ps1 -Serve
```

Open:

```text
http://127.0.0.1:8080/
```

Explain: Recovery Center Pro reads exported JSON. It cannot write the disk,
call OS APIs, or apply repairs. Buttons copy commands only.
