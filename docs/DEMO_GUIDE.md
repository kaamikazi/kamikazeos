# KamikazeOS v1.3 Demo Guide

This guide is for a public, portfolio, or reviewer demo. It avoids claims that
the project does not implement.

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
.\scripts\run.ps1
```

## 5. Shell Walkthrough

Inside KamikazeOS:

```text
about
health
store
journal
snapshots
devices
ghostattach
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

## 6. Analyze

After `export-logs`:

```powershell
python ai-recovery\analyze_logs.py build\serial.log
```

The analyzer reads the report and recommends only commands for IDs reported by
the kernel. It does not apply anything.

## 7. Dashboard

```powershell
python ai-recovery\export_dashboard_json.py build\serial.log recovery-center\data\recovery-report.json
```

Open:

```text
recovery-center/index.html
```

The dashboard is static and read-only. Copy buttons copy shell commands only.

## Optional Helper

```powershell
.\scripts\demo-v13.ps1
```

This helper builds, validates packages, imports PKG-101 when possible, and
prints the exact shell and host commands for the rest of the demo.
