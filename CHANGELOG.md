# Changelog

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
