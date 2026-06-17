# Recovery Flow

KamikazeOS recovery is evidence-first and human-approved.

## Action Flow

```text
kernel logs issue
  -> suggestion appears in shell/report
  -> human reviews action
  -> approve ACT-xxx
  -> policy gate decides
  -> reversible state change is applied
  -> journal records result
  -> rollback ACT-xxx remains available when safe
```

The analyzer may recommend `approve ACT-xxx`, but only for IDs already
reported by the kernel action catalog.

## Package Flow

```text
manifest imported or seeded
  -> kernel validates checksum, trust, signature metadata, payload, risk
  -> package appears as allowed or blocked
  -> human runs approve-pkg PKG-xxx
  -> human runs apply-pkg PKG-xxx
  -> metadata switches to inactive A/B slot
  -> human runs verify-pkg PKG-xxx
  -> package becomes verified or failed
```

Rollback:

```text
rollback-pkg PKG-xxx
```

## Report Flow

```text
export-logs
  -> kernel emits serial boot report
  -> ai-recovery/analyze_logs.py reads build/serial.log
  -> export_dashboard_json.py creates Recovery Center data
  -> recovery-center/index.html displays read-only state
```

No part of the analyzer or dashboard applies repairs.
