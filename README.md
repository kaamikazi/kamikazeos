# KamikazeOS

KamikazeOS is an experimental toy operating system that explores a narrow,
auditable version of "self-healing" infrastructure: the kernel records what
went wrong, exports structured reports, and allows only human-approved,
policy-gated recovery actions and simulated package updates.

This is not a production OS. It is a learning and portfolio project. It runs
in QEMU, has no user isolation, no networking stack, no real driver loading,
and no production security guarantees.

Current release: v1.6.0 "Kernel Discipline". It adds real x86 interrupt and
exception foundations while preserving the constrained recovery and update
model from the public demo releases.

## Feature Overview

- Bootable 32-bit x86 kernel in QEMU.
- Real x86 IDT, CPU exception stubs, legacy PIC remap, and PIT timer ticks.
- VGA text console and PS/2 IRQ keyboard shell with polling fallback.
- GhostAttach device/driver recovery layer with verified fallback policy.
- Structured kernel error logs and serial boot reports.
- RecoveryMemory with CMOS fallback.
- RecoveryStore on a raw ATA PIO disk image.
- Durable journal, boot snapshots, and health scoring.
- Fixed RecoveryEngine action catalog with policy gates.
- Package lifecycle: approve, apply, verify, rollback.
- Disk-backed package manifests and persistent package state.
- A/B module slot metadata for simulated package application.
- Offline package manifest tooling with real host-side Ed25519 signatures.
- Analyzer and read-only Recovery Center dashboard.

## Architecture

```mermaid
flowchart TD
  QEMU["QEMU i386"] --> Kernel["KamikazeOS kernel"]
  Kernel --> VGA["VGA text console"]
  Kernel --> IRQ["IDT + PIC + PIT"]
  IRQ --> Keyboard["PS/2 IRQ keyboard shell"]
  Kernel --> Ghost["GhostAttach"]
  Kernel --> Errors["Structured error log"]
  Kernel --> Store["RecoveryStore: ATA PIO recovery.img"]
  Store --> Journal["Durable journal"]
  Store --> Snapshots["Boot snapshots"]
  Store --> Manifests["Package manifest sectors"]
  Kernel --> Packages["Package lifecycle and A/B slot metadata"]
  Errors --> Serial["COM1 serial report"]
  Serial --> Analyzer["ai-recovery/analyze_logs.py"]
  Serial --> Exporter["export_dashboard_json.py"]
  Exporter --> Dashboard["Recovery Center static dashboard"]
  Tools["tools/package-builder"] --> Manifests
```

## Quickstart

Requirements on Windows:

```powershell
winget install LLVM.LLVM
winget install SoftwareFreedomConservancy.QEMU
winget install Python.Python.3.12
```

Build:

```powershell
.\scripts\build.ps1
```

Run the OS:

```powershell
.\scripts\run.ps1
```

Prepare a demo checklist:

```powershell
.\scripts\demo-v15.ps1
```

Analyze the captured serial report after running `export-logs` in the OS:

```powershell
python ai-recovery\analyze_logs.py build\serial.log
```

Generate dashboard data:

```powershell
python ai-recovery\export_dashboard_json.py build\serial.log recovery-center\data\recovery-report.json
```

Open:

```text
http://127.0.0.1:8080/
```

Serve the dashboard locally:

```powershell
.\scripts\generate-dashboard.ps1
```

## Demo Flow

Inside KamikazeOS:

```text
health
store
snapshots
packages
manifests
trust
package PKG-101
approve-pkg PKG-101
apply-pkg PKG-101
verify-pkg PKG-101
export-logs
```

On the host:

```powershell
python tools\package-builder\validate_manifest.py packages\examples\PKG-101-ghostattach-policy.json
python tools\package-builder\validate_manifest.py packages\examples\PKG-199-blocked-driver-canary.json
python ai-recovery\analyze_logs.py build\serial.log
python ai-recovery\export_dashboard_json.py build\serial.log recovery-center\data\recovery-report.json
```

See [docs/DEMO_GUIDE.md](docs/DEMO_GUIDE.md) and
[docs/demo-script.md](docs/demo-script.md) for the public demo flow.

## Screenshots

Recommended screenshots are listed in
[docs/SCREENSHOT_CHECKLIST.md](docs/SCREENSHOT_CHECKLIST.md). Existing
historical screenshots live under [docs/screenshots](docs/screenshots).

## Real vs Simulated

Real:

- Bootable kernel, shell, VGA, keyboard, serial output.
- ATA PIO disk persistence and durable journal.
- Deterministic health snapshots and reports.
- Host-side package manifest creation, validation, import/export.
- Real host-side Ed25519 manifest signing and verification.
- Kernel-side manifest checksum, trust-anchor, and policy enforcement.
- Read-only analyzer and Recovery Center Pro dashboard.

Simulated:

- In-kernel Ed25519 verification.
- Binary patching and real module replacement.
- Internet update fetching.
- AI auto-repair.
- Driver downloads or arbitrary package execution.

## Safety Principles

- No networking.
- No arbitrary package loading.
- No binary patching or kernel self-modification.
- Unknown, unsigned, invalid, high-risk, irreversible, checksum-failing, or
  trust-failing packages are blocked.
- Analyzer and dashboard are read-only and may only recommend or copy shell
  commands.
- The kernel is the source of truth for package policy.

## Version History

- v1.6.0 "Kernel Discipline": IDT, CPU exceptions, PIC, PIT, and IRQ keyboard.
- v1.5.0 "Public Demo Release": stable public demo docs and helper scripts.
- v1.4.0 "Recovery Center Pro": polished read-only dashboard UI.
- v1.3.0 "Public Release": documentation, demo, release polish.
- v1.2.0 "Trust Forge": offline signing pipeline and trust metadata.
- v1.1.0 "Offline Package Manifest Tooling": host import/export tools.
- v1.0.0: disk-backed manifests, package state, A/B slot metadata.
- v0.9.0 "Controlled Burn": package lifecycle and policy gates.
- v0.8.0 "Vital Signs": boot snapshots and health scoring.
- v0.7.0 "Black Box": disk-backed RecoveryStore and durable journal.

See [docs/VERSION_HISTORY.md](docs/VERSION_HISTORY.md).

## Roadmap

- Possible future technical work: virtual memory foundations, scheduler
  groundwork, real in-kernel Ed25519 verification, and richer test automation.
- Still out of scope: networking, driver downloads, binary patching, and
  autonomous repair.

## Portfolio Description

KamikazeOS is a small x86 toy OS built to demonstrate operating-systems
fundamentals, recovery-system design, update policy, offline signing tooling,
and honest AI-adjacent safety boundaries. It is deliberately constrained:
the interesting part is not that the OS can magically fix itself, but that it
can explain state, preserve evidence, reject unsafe changes, and keep humans
in control.
