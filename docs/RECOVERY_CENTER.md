# Recovery Center Pro

Status: v1.4.0 "Recovery Center Pro".

Recovery Center is a static, read-only dashboard for KamikazeOS exported boot
reports. It is a product UI around the existing recovery data; it is not an OS
control plane.

## Safety Contract

Recovery Center may:

- read `recovery-center/data/recovery-data.js`
- read `recovery-center/data/recovery-report.json`
- render tables, cards, charts, and timelines
- copy allowlisted shell command text to the clipboard

Recovery Center must never:

- modify `build/recovery.img`
- apply repairs
- call OS APIs
- download packages
- load drivers
- patch binaries
- execute arbitrary commands
- change KamikazeOS state

Recommended command buttons only copy text such as:

```text
approve-pkg PKG-101
verify-pkg PKG-101
approve ACT-004
```

The human still decides whether to run anything inside the KamikazeOS shell.

## Generate Data

Boot KamikazeOS, then run this in the OS shell:

```text
export-logs
```

On the host:

```powershell
python ai-recovery\export_dashboard_json.py build\serial.log recovery-center\data\recovery-report.json
```

With GNU make:

```powershell
make dashboard-json
```

The exporter writes both:

- `recovery-center/data/recovery-report.json`
- `recovery-center/data/recovery-data.js`

The `.js` companion lets the dashboard work from `file://` without a local web
server.

## Open The Dashboard

Open:

```text
recovery-center/index.html
```

If the page is blank or shows no data, regenerate dashboard data after booting
KamikazeOS.

## Panels

- System status header: report version, backend, boot count, journal count.
- Health score: current score and penalty breakdown.
- Health trend: boot snapshot score history.
- Storage backend: disk, state validity, durable boot count, CMOS fallback.
- Boot safety: safe mode, previous panic, rollback warnings.
- Trust and signatures: trust anchor, fingerprint, signature mode.
- Analyzer insights: offline findings derived from the serial report.
- Recommended commands: copy-only action/package command chips.
- GhostAttach devices: device status and attached driver.
- Driver registry: driver verification, risk, and load state.
- Recovery actions: fixed catalog statuses.
- Recovery journal: durable event timeline.
- Boot snapshots: per-boot health timeline.
- Package manifests: source, checksum, trust, signature, policy.
- Package state: persisted package lifecycle flags.
- Module A/B slots: active slot, rollback slot, pending verification.
- Recovery packages: policy-gated package registry.
- Warnings and errors: report events with analyzer explanations.

## Blank Dashboard Fixes

If the dashboard says `Run make dashboard-json after booting KamikazeOS`:

1. Boot KamikazeOS.
2. Run `export-logs` in the shell.
3. Run `make dashboard-json` or the Python exporter command on the host.
4. Refresh `recovery-center/index.html`.

If the dashboard still shows old data, confirm `build/serial.log` contains the
latest report and rerun the exporter.
