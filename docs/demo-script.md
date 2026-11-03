# KamikazeOS 2-Minute Demo Script

## Opening

KamikazeOS is a bootable x86 operating system recovery prototype that explores
safe recovery and update systems. KamikazeOS is not production-ready and not a
general-purpose operating system. The point is the control plane: evidence,
policy gates, human approval, durable history, and read-only analysis.

## Demo Beats

1. Boot KamikazeOS in QEMU.
2. Run `about` to show the v1.5 public demo release.
3. Run `health` to show deterministic health scoring.
4. Run `store` and `journal` to show ATA PIO-backed durable recovery state.
5. Run `trust` to explain real host-side Ed25519 signing and simulated
   kernel-side Ed25519 math.
6. Run `manifests`, `packages`, and `package PKG-101`.
7. Run:

```text
approve-pkg PKG-101
apply-pkg PKG-101
verify-pkg PKG-101
```

8. Run `export-logs`.
9. On the host, run:

```powershell
.\scripts\generate-dashboard.ps1 -Serve
```

10. Open Recovery Center Pro and show that command chips copy text only.

## Closing

The real work is boot, shell, storage persistence, journaling, snapshots,
offline signing tools, analyzer, and dashboard. The simulated parts are
explicit: binary patching, actual module replacement, kernel-side Ed25519 math,
internet updates, and AI auto-repair.
