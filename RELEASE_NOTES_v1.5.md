# KamikazeOS v1.5.0 "Public Demo Release"

v1.5.0 is the stable public demo release for KamikazeOS. It packages the
existing OS, offline recovery tooling, and Recovery Center Pro dashboard into a
cleaner demo workflow.

## Highlights

- Updated README and release documentation.
- Added `scripts/demo-v15.ps1` for demo preparation and command guidance.
- Added `scripts/run-demo.ps1` as a QEMU demo launcher.
- Added `scripts/generate-dashboard.ps1` to run analyzer/dashboard export and
  optionally serve Recovery Center.
- Added public demo artifacts:
  - `docs/demo-script.md`
  - `docs/demo-flow.md`
  - `docs/portfolio-summary.md`
- Refreshed screenshot and real-vs-simulated docs.

## Real

- Bootable toy OS in QEMU.
- VGA/keyboard shell.
- ATA PIO RecoveryStore.
- Durable journal.
- Boot snapshots and health scoring.
- Offline analyzer.
- Recovery Center Pro dashboard.
- Offline manifest tooling.
- Real host-side Ed25519 signing and verification.

## Simulated

- Binary patching.
- Actual module replacement.
- Kernel-side Ed25519 math.
- Internet update fetching.
- AI auto-repair.

## Safety

No new dangerous functionality was added. v1.5 still has no networking, package
downloads, driver downloads, auto-repair, arbitrary code execution, binary
patching, or kernel self-modification.
