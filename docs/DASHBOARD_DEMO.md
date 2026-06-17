# Dashboard Demo

This flow demonstrates Recovery Center Pro without adding any unsafe behavior.

## 1. Build

```powershell
.\scripts\build.ps1
```

If PowerShell script execution is blocked:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1
```

## 2. Boot KamikazeOS

```powershell
.\scripts\run.ps1
```

## 3. Export Logs Inside KamikazeOS

Run:

```text
health
store
snapshots
trust
packages
manifests
export-logs
```

## 4. Generate Dashboard Data

```powershell
python ai-recovery\export_dashboard_json.py build\serial.log recovery-center\data\recovery-report.json
```

or:

```powershell
make dashboard-json
```

## 5. Open Recovery Center

Open:

```text
recovery-center/index.html
```

## Demo Notes

- The dashboard is read-only.
- Command chips copy text only.
- The dashboard does not write `recovery.img`.
- The dashboard does not call KamikazeOS.
- The dashboard does not apply repairs.
- Package application remains simulated metadata only.

## Suggested Walkthrough

1. Start with the read-only safety banner.
2. Show the health card and trend chart.
3. Show backend state and durable journal count.
4. Show safe mode / panic warnings.
5. Show GhostAttach device and driver status.
6. Show trust/signature metadata and explain host Ed25519 vs simulated kernel
   Ed25519.
7. Show package manifests and policy results.
8. Show recommended commands and click one copy chip.
9. Paste or type the command manually inside KamikazeOS only if you intend to
   demonstrate the package lifecycle.
