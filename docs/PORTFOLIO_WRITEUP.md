# KamikazeOS Portfolio Writeup

KamikazeOS is an experimental x86 toy operating system focused on safe recovery
and update-system design. It boots in QEMU, provides a VGA/keyboard shell,
records structured kernel events, persists recovery state to a raw disk image,
and exports machine-readable reports for offline analysis.

The interesting part of the project is the constraint model. Instead of giving
an AI or update system broad power, KamikazeOS uses fixed catalogs, policy
gates, durable journals, and human approval. Unsafe packages are blocked by
construction: unknown, unsigned, invalid, high-risk, irreversible, checksum-
failing, and trust-failing manifests cannot move through the lifecycle.

What is real:

- Bootable 32-bit kernel.
- Device/driver recovery policy.
- Disk-backed recovery state and durable journal.
- Boot health snapshots.
- Offline manifest tooling.
- Real host-side Ed25519 signing and verification.
- Kernel-side checksum/trust/policy enforcement.
- Read-only analyzer and dashboard.

What is simulated:

- Binary patching.
- Actual module replacement.
- In-kernel Ed25519 cryptographic verification.
- Internet update fetching.
- AI auto-repair.

The project demonstrates operating-systems fundamentals, update pipeline
design, recovery observability, policy enforcement, and careful communication
about safety boundaries. It also shows the value of building the audit trail
before building automation.
