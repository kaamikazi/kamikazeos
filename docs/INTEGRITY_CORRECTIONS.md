# v2.6.1 Integrity Correction Record

Date: 2026-10-14 (Asia/Dhaka).
Branch: `hardening/v2.6.1-integrity-gate`.
Starting HEAD: `3b0e187`; tagged technical baseline: `f6c1814`.
The initial worktree was clean. This record describes implementation review
and tests by the implementing agent; independent release re-review is pending.

## Architecture Checked

The Multiboot x86 kernel initializes VGA/serial/logging, kernel GDT, exception
IDT and TSS before RecoveryMemory/ATA/RecoveryStore. It then initializes the
memory map/page tracker, bounded heap, validated paging, TinyFS, PIC/PIT/IRQs,
tasks, apps, syscall/privilege checks, held Ring 3 state, keyboard, GhostAttach,
recovery policy and package metadata. Reports precede clean-boot persistence
and snapshot saving; the interactive shell runs at CPL0.

The task model uses cooperative built-in steps, and apps are fixed kernel
entries. TinyFS is bounded and fixed-sector. Package application changes
version/A/B metadata rather than executable code. Host signing and verification
use real Ed25519; the kernel signature marker check remains simulated.

Python tools consume serial section/field reports, and the static Recovery
Center consumes exported JSON. Neither analyzer nor dashboard applies repairs.
The test runner uses disposable images, monitor keyboard input, serial evidence,
and before/after hashes of the primary recovery image.

## Actual Disk Layout

| LBA | Owner |
|---|---|
| 0 | untouched |
| 1 | RecoveryStore primary state, v1 fields plus generation/CRC extension |
| 2-15 | append-only recovery journal |
| 16-31 | 16 boot snapshot sectors |
| 32-39 | eight package manifest sectors |
| 40-47 | reserved |
| 48-49 | package state primary/mirror |
| 50 | RecoveryStore mirror when sector exists |
| 51-55 | reserved |
| 56 | module A/B slot record; 57-63 reserved |
| 64 | TinyFS superblock |
| 65-72 | TinyFS file table |
| 73-127 | TinyFS data |

All direct ATA write callers were inspected: RecoveryStore, snapshots,
package manifests/state, module slots, and TinyFS. ATA bounds checks use the
capacity retained from IDENTIFY. Corrections change no sector format or owner.

## Reproductions And Corrections

| ID | Evidence and root cause | Correction | Commit |
|---|---|---|---|
| D1 | Real QEMU boots on 32/50-sector images reported `state_locked=1`: init always read mirror LBA 50. 51/2048-sector controls passed. | Check IDENTIFY capacity before mirror access. Preserve validated primary/CRC handling in `single_copy_capacity`; failed reads of existing sectors still lock. | `47a6221` |
| D2 | Equal-generation conflict and primary/mirror read failures rewrote boot-1 snapshot bytes; journal read failure also changed snapshots. Init returned before reading journal boot evidence; snapshot saving only checked ATA availability. | Scan readable journal evidence even on state failure; retain a diagnostic boot lower bound. Lock state/journal on incomplete scans. Gate snapshot writes on locks, trusted order and durable state; preflight all history; hold at 8-bit boot exhaustion. | `8185f38` |
| L1 | A legacy primary beside a generation-500 mirror produced generation 3. | Keep legacy flags but advance from the valid mirror's generation floor; confirm restart and downgrade/upgrade behavior. | `2c4e4d6` |
| L2 | Host fixture compiling actual registry/log/health code showed score 80 rather than 75 after blocked exact driver plus verified fallback. | Classify block evidence after final device state; keep device penalties single-counted when missing/isolated. | `6d1c905` |
| L3 | Static review showed the intentional flag survived if a fault trigger returned. Actual CPU fault instructions normally halt, so this exceptional return was not induced in production code. | A compiled returning-trigger fixture tests the return-safe wrapper; actual Ring 0 fault commands verify persistence independently. | `9a4db22` |

QEMU read failures are injected through `blkdebug`, on the raw driver's file
child and primary IDE unit 0. This configuration follows the event placement
in [QEMU raw-format.c](https://github.com/qemu/qemu/blob/master/block/raw-format.c).
Images remain ordinary disposable raw files. The fixture asserts a detected
store lock as well as byte-for-byte evidence preservation.

## Test Evidence

Evidence is retained under `build/test-runs/`; these generated files are ignored.

| Run | Result |
|---|---|
| `integrity-takeover-baseline` | original suite: 32 passed, 0 failed, 1 skipped (baseline ELF not supplied) |
| `integrity-defects-before` | D1, D2 conflict, and L1 reproduced on saved pre-correction ELF; initial read-error fixture was subsequently corrected |
| `integrity-io-confirm-before` | 3 genuine existing-sector read failures reproduced snapshot mutation; 0 passed, 3 failed as expected before repair |
| `integrity-d1-fixed` | 4 passed; two restarts each at 32/50/51/2048 sectors |
| `integrity-d1-broad` | 18 passed, 0 failed, 1 skipped |
| `integrity-d2-final` | 26 passed, 0 failed, 0 skipped; includes 7 snapshot cases and tagged downgrade/re-upgrade |
| `integrity-l1-fixed` | 10 passed, 0 failed, 0 skipped |
| `integrity-l2-before` | 2 passed, 1 failed, confirming L2 in actual C policy code |
| `integrity-l2-broad` | 15 passed, 0 failed, 0 skipped |
| `integrity-l3-final` | 5 passed: 2 host scope cases and 3 real Ring 0 exceptions |
| `integrity-corrections-final` | **53 passed, 0 failed, 0 skipped in 299.100 seconds**, including all 33 original tests and 20 added regressions |

The initial L3 fault assertions expected decimal vectors, while actual panic
evidence uses hexadecimal. The harness was corrected and rerun; these initial
harness failures are not counted as passing kernel tests.

Full Windows builds, Python compilation, JavaScript syntax checks, PKG-101
positive validation and PKG-199 negative validation were executed. The complete
suite includes analyzer execution, dashboard JSON generation, headless DOM
rendering, TinyFS persistence, recovery-action persistence, package ownership,
manifest checks, durable journal behavior, health and held privilege checks.
No browser visual QA is claimed for this run.

## Primary Image And Ring 3

Primary `build/recovery.img` SHA-256 before the work:

```text
4daf871c5c2982a2c6401a8a3336959f31bdd0143ae00f238f4cbdc7f453f19e
```

The final post-run hash matches exactly. Every completed regression run
retained this hash. The original image was
never attached to QEMU, formatted, reset or deleted.
`RING3_ROUNDTRIP_TEST_APPROVED=0` and `execution_approved=no` are preserved.
All exception tests originate at CPL0. No CPL3 transition was performed.

## Compatibility And Remaining Risks

Verdict: no migration required. v1 state/checksum fields, extension bytes and
existing storage owners are unchanged. Small-disk single-copy state and
51/2048-sector mirrored state pass restart tests. TinyFS and package storage
require their own sector ranges; small recovery disks do not provide them.

The snapshot ring remains single-sector metadata rather than a journaled
transaction. CRC/checksums detect accidental damage, not hostile modification.
Fault injection covered between-boot corruption and live read failures, not
power interruption during an ATA sector write/flush. Journal appends still
rewrite their containing sector; physical torn-sector atomicity is not proven.
Small disks have no mirror redundancy. Corrupt or future snapshot history
intentionally holds new snapshot writes until manually investigated. Boot
number widening still needs a format migration; exhaustion now holds writes.
Legacy `clear-store` does not propagate every failed reset write. PS/2 behavior
outside tested QEMU remains incompletely validated. Kernel Ed25519 is simulated.

NULL-page protection, CR0.WP, read-only kernel text, guarded stacks and improved
double-fault handling belong to the proposed v2.6.2 review and are not included.

Implementation recommendation: **ACCEPT** for the specified integrity
correction scope, backed by the 53-test result. Independent release re-review
remains pending. This document does not approve Ring 3 execution, a merge,
or a push.

## Files Changed

- Kernel: `recovery_store.c/.h`, `recovery_memory.c/.h`, `boot_snapshot.c`,
  `report.c`, `ghostattach.c`, `panic.h`, new `panic_test.c`, `shell.c`, `ring3.c`.
- Test harness: `kmz_qemu.py` (block-layer read failures) and `kmz_disk.py`
  (generation-aware fixtures).
- Added QEMU tests: `test_small_disk.py`, `test_integrity_corrections.py`,
  `test_legacy_generation.py`, `test_panic_scope.py`.
- Added host fixtures: `test_health_policy.py`, `health_policy_fixture.c`,
  `panic_scope_fixture.c`, `kernel_fixture.js`.
- Documentation: README, CHANGELOG, v2.6.1 release notes, architecture, storage
  integrity, health model, learning guide, tests README and this record.

## Commands Executed

```powershell
python tests/run_regression.py --run-dir build/test-runs/integrity-takeover-baseline
python tests/run_regression.py -k D1SmallDisk --run-dir build/test-runs/integrity-d1-fixed
python tests/run_regression.py -k D2 -k F1 -k D1 -k R1 --baseline-elf build/integrity-downgrade-baseline/build/kamikaze.elf --run-dir build/test-runs/integrity-d2-final
python tests/run_regression.py -k L1 -k F1 --baseline-elf build/integrity-downgrade-baseline/build/kamikaze.elf --run-dir build/test-runs/integrity-l1-fixed
python tests/run_regression.py -k L2 -k F4 -k R1 --run-dir build/test-runs/integrity-l2-broad
python tests/run_regression.py --no-build -k L3 --run-dir build/test-runs/integrity-l3-final
python tests/run_regression.py --no-build --baseline-elf build/integrity-downgrade-baseline/build/kamikaze.elf --run-dir build/test-runs/integrity-corrections-final
python -m compileall -q ai-recovery tools/package-builder tests
node --check recovery-center/app.js
python tools/package-builder/validate_manifest.py packages/examples/PKG-101-ghostattach-policy.json
python tools/package-builder/validate_manifest.py packages/examples/PKG-199-blocked-driver-canary.json
git diff --check
```

Build-enabled runs invoke `scripts/build.ps1` through Windows PowerShell.
The baseline source was exported with `git archive f6c1814` and built under
`build/integrity-downgrade-baseline/`; no worktree reset was used. The saved
pre-correction ELF independently reproduces the reported defects. Analyzer
and dashboard exporter commands are executed by the R1 regression and their
output is retained in the per-test directory.
