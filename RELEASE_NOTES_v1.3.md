# KamikazeOS v1.3.0 "Public Release"

v1.3.0 is the public demo and documentation release for KamikazeOS.

This release makes the project easier to understand, reproduce, present, and
review. It does not add new dangerous functionality. v1.2.0 "Trust Forge"
remains the latest technical feature milestone.

## Highlights

- Public README refresh.
- Architecture documentation.
- Safety principles and real-vs-simulated documentation.
- Command reference.
- Recovery flow documentation.
- Demo guide and screenshot checklist.
- Portfolio writeup and LinkedIn draft.
- Troubleshooting guide.
- Release metadata and changelog.

## Safety

Still not implemented:

- networking
- binary patching
- driver downloads
- arbitrary code execution
- AI auto-repair
- kernel self-modification

The analyzer and Recovery Center remain read-only.

## Technical Milestone Reminder

v1.2 "Trust Forge" added the latest core update-pipeline work:

- real host-side Ed25519 manifest signing and verification
- deterministic signed payloads
- disk manifest import/export
- kernel-side checksum and trust enforcement
- package policy rejection

Kernel-side Ed25519 cryptographic verification remains simulated and labeled.
