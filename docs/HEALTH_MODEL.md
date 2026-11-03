# Health Score Model (v2.6.1)

```
health = 100
       - 15  if the previous boot panicked (unless it was an intentional test)
       - 10  per device with no verified driver        (device state)
       -  8  per isolated production device            (device state)
       -  5  per PRODUCTION warning logged this boot
       - 25  per PRODUCTION error or panic logged this boot
```

The score is clamped at 0. The report's `[CURRENT_HEALTH]` section shows
every term plus `health_model=production_events_per_boot`.

## What changed from v2.6.0

v2.6.0 counted every warning and critical currently in the 64-entry log
ring. So:

- A clean boot scored 55: compiled-in canaries cost 25 points.
- Every shell typo cost 5 points. Five typos between `apply-pkg` and
  `verify-pkg` failed a valid package.
- Logging enough harmless events to wrap the ring *raised* the score
  (30 to 80 in the reproduction).

v2.6.1 counts events with **per-boot cumulative counters** that never wrap,
and only events classified as production faults.

## Event classes

Every event is still logged live on serial, kept in the ring, and exported
in the report. A non-production event carries a `;health=<class>` suffix
(the analyzer shows it). The class only decides whether the event costs
health.

| Class | Meaning | Events |
|---|---|---|
| `production` (default) | a real fault | everything not listed below |
| `excluded_operator_input` | a typo, invalid argument, or operator request refused by policy | `SHL-001`; `approve ACT-999` (`REC-BLOCK-999`); `approve-pkg` of a blocked package (`PKG-BLOCK-001`); `PKG-REJECT-001/002`; `RING3-102`; `SYS-ERR-001/002/004` from the Ring 0 shell `syscall` command; `APP-BLOCK-001`; TinyFS request validation `TFS-ERR-001/002/004/005/006/008/009/010` |
| `excluded_expected_canary` | a compiled-in canary blocked as designed | `REC-BLOCK-999` for ACT-999 at boot; `PKG-BLOCK-001` at boot for a built-in seed (PKG-003) |
| `excluded_intentional_test` | an operator-triggered test fault | `panic-test`, `exceptions-test ... confirm`, `pagefault-test confirm`, `user-fault-test confirm`, and the next boot's `BOOT-WARN-001` for such a panic |
| `scored_via_device_state` | already penalised through final device status | `GA-DRV-101`; `GA-SEC-001` only when the final device is missing, isolated, or failed |

Unclassified call sites default to `production`, so a new fault can never
be excluded by accident.

### Still production faults (deliberately)

- A **blocked imported** manifest (`PKG-BLOCK-001` for `disk_imported`).
- Driver attach failures (`GA-DRV-201`).
- An unverified exact driver blocked before a verified fallback succeeds:
  `GA-SEC-001` remains a production warning because the final usable device
  has no missing-driver penalty. Classification happens after attachment.
- Store and journal integrity events (`STORE-*`), including the single
  `STORE-WARN-004` per boot while the journal is full.
- TinyFS write/read/integrity faults (`TFS-ERR-003`, `-011`, `-012`, `-013`,
  `-014`, `-016`, `-017`) and all `TFS-WARN-*`.
- Syscall rejections that arrive from CPL3 or with an invalid origin.
- Kernel heap rejections (`HEAP-101/102`). The allocator cannot tell an
  operator `heap-alloc` request from a kernel caller, so these stay
  conservative.
- Any panic or exception not preceded by a test command.

## Intentional test panics across reboots

A test command calls `kpanic_expect_intentional()` immediately before it
triggers the fault. The panic is then:

- persisted with an "intentional" flag (CMOS flags bit 5, and the disk
  state extension flag),
- journaled as `PANIC` with code `PANICT`,
- reported on the next boot as `last_boot_panic=1` and
  `last_boot_panic_intentional=1`.

A panic recorded by an older kernel, or without the flag, is always
treated as real (-15).

Built-in divide, invalid-opcode and page-fault triggers use
`kpanic_run_intentional_test()`. If a trigger unexpectedly returns, the wrapper
clears intentional classification before logging `TEST-RETURN-001` as a
production error. The disabled CPL3 fault path also clears classification on
refusal or unexpected return. Normal fault persistence remains unchanged.

## Package verification

`apply-pkg` records production criticals and health. `verify-pkg` fails
if production criticals increased or health fell by more than 20.
Operator typos and other excluded events cannot fail a valid package.
