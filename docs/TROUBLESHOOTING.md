# Troubleshooting

## clang Not Found

Install LLVM:

```powershell
winget install LLVM.LLVM
```

Then open a new terminal so PATH is refreshed.

## QEMU Not Found

Install QEMU:

```powershell
winget install SoftwareFreedomConservancy.QEMU
```

## No Serial Report Found

The analyzer needs a complete report in `build/serial.log`.

Inside KamikazeOS, run:

```text
export-logs
```

Then rerun:

```powershell
python ai-recovery\analyze_logs.py build\serial.log
```

## Dashboard Shows Old Data

Regenerate the dashboard JSON:

```powershell
python ai-recovery\export_dashboard_json.py build\serial.log recovery-center\data\recovery-report.json
```

Then refresh `recovery-center/index.html`.

## PKG-101 Is Missing

Import it into the recovery image:

```powershell
.\scripts\import-example-package.ps1
```

Then reboot KamikazeOS and run:

```text
manifests
packages
package PKG-101
```

## PKG-199 Fails Validation

That is expected. PKG-199 is a blocked-driver canary. It is high risk,
irreversible, unsigned, and uses a blocked payload type.

## recovery.img Contains Old State

`build/recovery.img` is durable state. It is intentionally preserved across
normal builds. Use destructive reset commands only when you intentionally want
a fresh demo disk.

Inside KamikazeOS:

```text
reset-packages confirm
```

The Makefile also has `reset-store`, but it deletes the recovery image and
should be used carefully.
