# KamikazeOS v2.1 Demo Guide

This guide is for a public, portfolio, or reviewer demo. It avoids claims that
the project does not implement.

## Fast Prep

```powershell
.\scripts\demo-v20.ps1
```

This builds the kernel, validates the safe and unsafe package examples, and
prints the exact QEMU, shell, analyzer, and dashboard steps for the integrated
v2.1 demo.

## 1. Build

```powershell
.\scripts\build.ps1
```

## 2. Validate Package Examples

Safe signed package:

```powershell
python tools\package-builder\validate_manifest.py packages\examples\PKG-101-ghostattach-policy.json
```

Expected: `result=PASS`.

Unsafe canary:

```powershell
python tools\package-builder\validate_manifest.py packages\examples\PKG-199-blocked-driver-canary.json
```

Expected: `result=FAIL` with unsafe reasons.

## 3. Import Safe Signed Manifest

```powershell
.\scripts\import-example-package.ps1
```

This writes PKG-101 into the package manifest slot range of
`build/recovery.img`. The kernel still validates it independently at boot.

## 4. Boot

```powershell
.\scripts\run-demo.ps1
```

## 5. Shell Walkthrough

Inside KamikazeOS:

```text
about
system
diagnose
health
store
journal
snapshots
devices
ghostattach
interrupts
ticks
mem
memmap
pages
tasks
task-current
fs
apps
run tinyfs-demo
cat appdemo.txt
trust
manifests
packages
package PKG-101
approve-pkg PKG-101
apply-pkg PKG-101
slots
verify-pkg PKG-101
package PKG-101
export-logs
```

Notes for narration:

- `apply-pkg` is metadata-only. It does not patch a binary.
- `verify-pkg` clears pending verification if health and slot metadata are OK.
- `trust` labels host Ed25519 as real and kernel Ed25519 as simulated.
- `system` and `diagnose` summarize integrated v2.1 readiness without applying
  repairs.
- `mem`, `memmap`, and `pages` show physical memory discovery and accounting,
  not virtual memory or heap allocation.
- `run tinyfs-demo` runs a fixed built-in kernel-managed app. It does not load
  executable code from disk.

## 6. Analyze

After `export-logs`:

```powershell
python ai-recovery\analyze_logs.py build\serial.log
```

The analyzer reads the report and recommends only commands for IDs reported by
the kernel. It does not apply anything.

## 7. Dashboard

```powershell
.\scripts\generate-dashboard.ps1 -Serve
```

For a smoke-test log:

```powershell
.\scripts\generate-dashboard.ps1 -LogPath build\serial-v20.log -Serve
```

Open:

```text
http://127.0.0.1:8080/
```

The dashboard is static and read-only. Copy buttons copy shell commands only.

## Optional Helper

```powershell
.\scripts\demo-v20.ps1
```

This helper builds, validates packages, and prints the exact shell and host
commands for the rest of the demo.
