# Focused module-slot persistence correction

Implementation evidence; not independent acceptance. Scope: per-module slot
write authorization and defensive regressions, starting at
`9d5f43e42e667390ad5fc2b69953b79e9753e5cd` on
`hardening/v2.6.1-integrity-gate`.

This document records the historical slot-only correction and 135-case snapshot.
The subsequent review found F1/F2 reset-exception gaps; their correction, the
complete 148-case result and current source receipts are documented in
[RESET_BOUNDARY_CORRECTION.md](RESET_BOUNDARY_CORRECTION.md). Earlier reset/API
claims below describe that historical snapshot, not verification of those gaps.

## Phase 1: unchanged-kernel reproduction

Added actual-C regressions before editing kernel behavior. They use real
authority, package-state, module-slot, version and lifecycle code. ATA outcomes,
manifest trust and diagnostics use the established controlled fixture. Stages
are saved through the actual state writer. Cold contexts use the existing
fixture's explicitly simulated new BSS; warm reload tests do not clear it.

Command:

```powershell
python -B tests/run_regression.py --no-build --run-dir "build/test-runs/slot-transition-before" -k T5SlotTransitionsC -k T4JointAuthorityC -k N4OwnershipCommit
```

Result: **20 cases, 14 passed / 6 failed, 0 errors/skips, 4.043 seconds**.
All ten existing ownership/latch/reset cases passed. Four new controls passed.
Six new negative cases failed:

- `test_apply_stage_cannot_reverse_acknowledged_switch`
- `test_verify_stage_cannot_reverse_acknowledged_verification`
- `test_rollback_stage_cannot_reverse_acknowledged_rollback`
- `test_one_modules_stage_cannot_authorize_another_modules_change`
- `test_mixed_authorized_and_foreign_changes_are_rejected_atomically`
- `test_rollback_cannot_change_inactive_verification_flag`

`git diff -- kernel` was empty after this run. Authority source Git blob:
`03d556ed14cc4bd9ffbecfb6e0acd93ae88b9bff`.

Baseline SHA-256 receipts:

| Artifact | SHA-256 |
|---|---|
| `tests/slot_transition_fixture.c` | `cf96cb0ce7123d6035526c9c68fa5c128426b52e7b8b75e658bbeb726aab7bbc` |
| `tests/test_slot_transitions.py` | `1a813f89e88091f9ed059f4d29859821d7b994c68436ba77a15bcdf6fc917810` |
| `build/test-runs/slot-transition-before/slot-transitions-actual/slot_transition_fixture.wasm` | `3b7875eed8d362fb0d5bb5e76cdc172d56ea3cc1eb8d2c5eb99baac6da1bf455` |
| `build/test-runs/slot-transition-before/summary.txt` | `83be75e0217a64de01fd242259bfc4a1fb04404b92fc14b1ec2a3215517d3852` |

The run directory retains compilation diagnostics and each case's output.
The pre-existing ELF reported SHA-256 prefix `9a323f8077b25700`; these host
checks compile the actual C sources separately rather than execute that ELF.

Preservation hashes at takeover:

- `build/recovery.img`: `4daf871c5c2982a2c6401a8a3336959f31bdd0143ae00f238f4cbdc7f453f19e`
- `audit-evidence/release-8e75093.zip`: `f5982850ab1d46a4ca4f0d9bdb09fab31311abc9e66f64387848ae8a10235365`

## Phase 2: root cause and enforced transition invariants

Previously, `package_authority_allow_slot_write()` accepted a coherent candidate
whenever the aggregate assessment found any staged module. Consistency with a
recoverable before/after shape did not prove permission to move from the current
durable slot image to that candidate. The whole-sector serializer therefore
could persist backward changes, unrelated-module changes and extra flag changes.

`slot_edges_valid()` now compares every proposed module with its acknowledged
durable record before any sector write. Unchanged records do not grant authority.
Every changed module needs its own uniquely matching, manifest-bound durable
stage and valid storage evidence. It constructs the narrowly permitted successor
from the old record and compares every serialized field/array byte, excluding C
struct padding:

| Phase | Required durable predecessor | Permitted changes |
|---|---|---|
| APPLY_STAGED | Approved, verified active before/from, not pending | Write inactive to-version/unverified; activate it; retain before as rollback; set pending |
| VERIFY_STAGED | Applied after/to with pending/unverified active | Set active verified; clear pending |
| ROLLBACK_STAGED | Current applied/verified/failed after/to shape | Activate before; clear rollback pointer and pending; preserve both versions and verification flags |

The stage phase and saved status must agree. An after-side predecessor cannot
move back to the before side under the old stage. Correctly staged changes to
two modules can proceed together; an unauthorized change anywhere rejects the
whole write before ATA I/O and leaves all durable records/media untouched.
Rejected proposals do not invalidate known-good durable authority. Exact
unchanged staged retries remain supported.

`staged_result()` still classifies evidence for consistency and reconciliation;
the independent transition check supplies mutation permission. No backward
slot-writing recovery path was necessary: existing reconciliation publishes the
observed package state without changing slots. Its before/after handling remains
covered for all three phases (six cold recovery combinations).

Explicit reset retains its separate RAM-only grant, requiring trusted non-staged
evidence and permitting only empty package state followed by factory slots.
Pristine initialization retains the positive all-zero disk-read exception; the
ordinary validator cannot advance changed records from diagnostic defaults,
empty slot evidence or uncertain storage. No public API or disk format changed.

## Phase 3: verification

Fresh kernel build completed successfully with the existing build script and
`-Wall -Wextra`. Actual-C compile diagnostics were empty. Final ELF SHA-256:

`f167624fd2f351295fdde181c5d9dbd5912648435acfdfb4cce8d542c4e9a1f4`

| Run directory under `build/test-runs/` | Passed | Failed | Errors | Skipped | Seconds |
|---|---:|---:|---:|---:|---:|
| `slot-transition-before` | 14 | 6 | 0 | 0 | 4.043 |
| `slot-transition-fixed-actual` | 20 | 0 | 0 | 0 | 3.356 |
| `slot-transition-focused` | 41 | 0 | 1 | 0 | 246.487 |
| `slot-transition-lifecycle-corrected` | 1 | 0 | 0 | 0 | 15.595 |
| `slot-transition-full` | **135** | **0** | **0** | **0** | **991.601** |

Runs overlap; their case counts must not be added as independent coverage.
The intermediate focused-run error was a new QEMU test's AI manifest setup:
it supplied an overlong component label and omitted the explicit target module.
The kernel correctly refused its checksum. Only fixture construction was fixed;
the corrected test then passed alone and in the complete run. All intermediate
logs are retained. No passing claim is made for the errored run.

The 135-case run retains all 122 existing cases and adds ten actual-C slot
authorization cases, one QEMU lifecycle/reset case and two journal-gap controls.
It covers:

- Forward apply/verify/rollback, A/B parity across sequential updates, legitimate
  two-module changes, and unchanged retries.
- Backward transition rejection for every phase, foreign-module rejection,
  whole-write rejection for mixed candidates and preservation of inactive flags.
- Six interrupted before/after reconciliation combinations without slot writes;
  real read/write failures, final-state publication failures, sticky warm reload
  holds and cold restoration of valid evidence.
- Authorized reset, partial-reset holds, reset/reboot/fresh apply, and degraded
  current ownership, including actual disposable-QEMU command execution.
- Missing canonical CLEAN_BOOT 17 despite malformed 17 plus valid 18. Boots
  19/20/21 append real later completions, but cannot fill the earlier gap: all
  snapshot sectors and every occupied journal-prefix byte remain unchanged.
  A matched canonical-17 control permits boot 19 to replace only aged boot 3.
- Existing compatibility, TinyFS, manifest, 18-boot ring rotation, Ring 0 fault
  persistence, host consumer and held Ring 3 controls.

Commands:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1
python -B tests/run_regression.py --no-build --run-dir "build/test-runs/slot-transition-fixed-actual" -k T5SlotTransitionsC -k T4JointAuthorityC -k N4OwnershipCommit
python -B tests/run_regression.py --no-build --run-dir "build/test-runs/slot-transition-focused" -k T5Slot -k T4JointAuthority -k N4OwnershipCommit -k F2PackageRollback -k T3JournalChronology -k T1SnapshotPlacement -k invalid_clean_does_not_age -k aborted_staged_apply -k switched_staged_apply -k degraded_current_apply
python -B tests/run_regression.py --no-build --run-dir "build/test-runs/slot-transition-lifecycle-corrected" -k T5SlotLifecycle
python -B tests/run_regression.py --no-build --baseline-elf "C:\Users\imran\AppData\Local\Temp\opencode\kmz-release-audit-9d5f43e\baseline\build\kamikaze.elf" --run-dir "build/test-runs/slot-transition-full"
```

Compatibility baseline provenance: commit
`f6c18148e6b003dfe48d3fdded3f83e603d95d2e`, independently exported/built during
the preceding review. Its ELF SHA-256 matched the preserved provenance receipt:
`5a49cd85cd34c23062c142bba2913a30d22159ccc3c56b0ab48c53e804be0b51`.

Final full-run summary SHA-256:
`07dd40f46ceb9d2222632640fc0ed74096dab6ec0a9110a12138a66093f4dfb9`.
The tested authority source Git blob is
`d76e46422b77b70c90bfbaf375a8852cd996ab41`. HEAD remains the starting commit;
the correction is uncommitted working-tree content. This source blob and ELF
identify the tested implementation without attributing it to the starting HEAD.

## Compatibility, preservation and limitations

- Existing slot/package encoders, sectors, fields, checksums and public APIs
  are unchanged. No migration, persistent transaction identity or recovery
  architecture redesign was introduced. Old serialized records remain readable;
  previously unauthorized candidate writes are now refused.
- `build/recovery.img` and `release-8e75093.zip` hashes match the takeover values
  above after all runs. The other original archive, `release-0cd86d8.zip`, was
  not modified; its final SHA-256 is
  `bae909075761af2c3edafe33e993002487d79cc61c9c0dae483ffccb2b75be75`.
- Original audit evidence and privilege source were not edited. The approval
  macro remains `RING3_ROUNDTRIP_TEST_APPROVED 0`. QEMU used only per-test
  disposable images; no CPL3 execution, merge or push occurred.
- Package operations remain metadata-only. Consistency checks do not authenticate
  hostile coherent disk rewrites or provide physical torn-write/flush atomicity.
  Rejection is atomic at the pre-I/O authorization boundary, not a new claim of
  hardware-sector atomicity. Host fixture ATA/manifest/health controls remain
  simulated; QEMU controls provide complementary real-kernel execution.

**Request a separate read-only review of this working-tree correction and its
evidence before acceptance. These results are implementing-agent verification,
not independent acceptance.**
