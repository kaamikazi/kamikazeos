# KamikazeOS v2.0 Demo Script

## Host Prep

```powershell
.\scripts\demo-v20.ps1
```

## Shell Flow

```text
system
diagnose
health
tasks
fs
apps
run tinyfs-demo
cat appdemo.txt
packages
trust
export-logs
```

## Host Analysis

```powershell
.\scripts\generate-dashboard.ps1 -LogPath build\serial.log -Serve
```

## Narration

KamikazeOS v2.0 is an experimental OS research prototype. It demonstrates a
bootable kernel, kernel diagnostics, durable recovery state, policy gates,
TinyFS, a fixed built-in app model, offline package trust tooling, analyzer
output, and a read-only dashboard. It is not production-ready and not a
general-purpose operating system.

The important safety boundary: all repair and update actions remain explicit,
logged, policy-gated, and human-approved.
