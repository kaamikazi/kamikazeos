# LinkedIn Post Draft

I built KamikazeOS v1.3.0 "Public Release", an experimental x86 operating
system research prototype focused on safe recovery and update-system design.

It boots in QEMU, has a VGA/keyboard shell, a GhostAttach device recovery
layer, structured kernel logs, serial boot reports, disk-backed recovery state,
a durable journal, boot health snapshots, and a read-only Recovery Center
dashboard.

The latest technical milestone, v1.2 "Trust Forge", added offline package
manifest tooling with real host-side Ed25519 signing and verification. The
kernel independently enforces checksums, trust-anchor identity, signature
metadata, and policy gates.

The safety boundary is the main point:

- no networking
- no driver downloads
- no arbitrary package execution
- no binary patching
- no kernel self-modification
- no AI auto-apply

Unsafe packages are blocked: unknown, unsigned, invalid, high-risk,
irreversible, checksum-failing, or trust-failing manifests cannot pass.

What is simulated is labeled clearly: in-kernel Ed25519 verification, actual
module replacement, and binary patching are not implemented. Package
application is metadata-only.

This project helped me connect OS fundamentals with update-system architecture:
boot flows, storage layout, structured diagnostics, durable state, recovery
policy, signing tooling, and honest product communication.
