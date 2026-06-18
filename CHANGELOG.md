# Changelog

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
