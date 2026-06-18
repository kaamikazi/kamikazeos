# Screenshot Checklist

## v1.5 Public Demo Release

Suggested release screenshots:

- QEMU boot screen with v1.5 banner.
- Shell `about`.
- Shell `help`.
- `health`.
- `store`.
- `journal`.
- `snapshots`.
- `trust`.
- `manifests`.
- `packages`.
- `package PKG-101`.
- `approve-pkg PKG-101`, `apply-pkg PKG-101`, `verify-pkg PKG-101` flow.
- Analyzer output.
- Recovery Center Pro top system status header.
- Recovery Center Pro health score and trend.
- Recovery Center Pro trust/signature panel.
- Recovery Center Pro package/manifest tables.
- Recovery Center Pro recommended command chips.

Do not imply browser visual QA was performed unless it actually was.

Suggested v1.3 public release screenshots:

- Boot screen.
- Shell `help`.
- `about`.
- `devices`.
- `ghostattach`.
- `health`.
- `store`.
- `journal`.
- `snapshots`.
- `trust`.
- `manifests`.
- `packages`.
- `package PKG-101`.
- `slots` after apply and before verify.
- `package PKG-101` after verify.
- Analyzer output.
- Recovery Center dashboard overview.
- Recovery Center package table.
- Recovery Center trust/signature panel.

Do not imply browser visual QA was performed unless it actually was.

## v1.4 Recovery Center Pro

Suggested v1.4 dashboard screenshots:

- Top system status header with read-only badge.
- Read-only safety banner.
- Health score card.
- Health trend chart.
- Backend/storage card.
- Safe mode / previous panic warning card.
- Trust and signature panel.
- Analyzer insights and recommended command chips.
- GhostAttach device table.
- Driver registry table.
- Recovery action table.
- Recovery journal timeline.
- Boot snapshots timeline.
- Package manifest table.
- Package state table.
- Module A/B slots table.
- Recovery packages table.
- Blank dashboard empty state.

For the blank state, temporarily move or rename
`recovery-center/data/recovery-data.js` and open the dashboard over a local
HTTP server with no JSON file available, then restore the data file.
