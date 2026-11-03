# KamikazeOS Portfolio Summary

## GitHub Project Description

KamikazeOS is a bootable x86 operating system recovery prototype for exploring
safe recovery and update-system design. It includes a VGA/keyboard shell,
GhostAttach device recovery policy, structured serial reports, ATA PIO-backed
RecoveryStore, durable journaling, boot health snapshots, offline package
manifest tooling, real host-side Ed25519 signing, and a read-only Recovery
Center Pro dashboard.

## What It Demonstrates

- OS boot flow and freestanding C.
- Basic device I/O: VGA, PS/2 keyboard, COM1 serial, ATA PIO.
- Recovery-state persistence without a filesystem.
- Durable evidence via append-only journaling.
- Health scoring and boot snapshots.
- Policy-gated recovery actions.
- Offline update manifest design.
- Host-side signing and validation.
- Read-only analyzer/dashboard product design.

## Honest Boundaries

Real:

- Bootable QEMU-based experimental kernel.
- Shell.
- ATA PIO RecoveryStore.
- Durable journal.
- Snapshots.
- Recovery Center dashboard.
- Host-side Ed25519 signing.
- Offline manifest tooling.

Simulated:

- Binary patching.
- Actual module replacement.
- Kernel-side Ed25519 math.
- Internet updates.
- Auto-repair.

## LinkedIn Post Draft

I built KamikazeOS v1.5.0 "Public Demo Release", a bootable x86 operating
system recovery prototype about safe recovery and update-system design.

It runs in QEMU with a VGA/keyboard shell, structured serial reports,
GhostAttach driver recovery policy, ATA PIO-backed durable recovery state,
append-only journaling, health snapshots, offline signed package manifests, and
a read-only Recovery Center Pro dashboard.

The safety model is the interesting part: no networking, no driver downloads,
no binary patching, no arbitrary package execution, no kernel self-modification,
and no AI auto-repair. The analyzer and dashboard can recommend or copy shell
commands, but a human must run them inside the OS.

Real: bootable QEMU-based experimental kernel, shell, ATA PIO persistence,
durable journal, snapshots, dashboard, offline manifest tooling, and host-side
Ed25519 signing.

Simulated and clearly labeled: binary patching, actual module replacement,
kernel-side Ed25519 math, internet updates, and auto-repair.

This project connected OS fundamentals with update pipeline design: evidence
first, policy gates second, automation last.
