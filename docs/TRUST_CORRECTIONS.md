# v2.6.1 architectural trust-boundary corrections

Implementation evidence, not independent release acceptance. Starting clean
HEAD: `8e75093f1d880d8b546d4f0c8f0d6e7ef9506086`, branch
`hardening/v2.6.1-integrity-gate`. Required invariants were written and committed
as `3b853e4` **before kernel implementation**; see `TRUST_INVARIANTS.md`.

Focused follow-up from `9d5f43e`: the review found that explicit transition-edge
checks covered package-state proposals but were missing from module-slot writes.
See `SLOT_TRANSITION_CORRECTION.md` for six reproduced failures, per-module
forward authorization, and the subsequent 135-case full run. The historical
passing results below did not establish that missing slot-writer property.
The two reset-boundary findings from the independent review of that correction
(F1 exact factory image, F2 one-shot reset grant) are recorded in
[RESET_BOUNDARY_CORRECTION.md](RESET_BOUNDARY_CORRECTION.md), including their
before/after reproductions and the complete **148 passed, 0 failed/errors/skips**
run (exit code 0). The older result tables below remain historical evidence.

## Preservation and unchanged-source reproduction

The complete newest independent audit is preserved outside temporary/build
storage in `audit-evidence/release-8e75093.zip`, 2,410 files / 289,238,824
uncompressed bytes. Every entry was checked against its original SHA-256.
ZIP SHA-256: `f5982850ab1d46a4ca4f0d9bdb09fab31311abc9e66f64387848ae8a10235365`.
Readable reports are in `audit-evidence/8e75093/`; the per-file inventory is
retained locally as `audit-evidence/inventory-8e75093.json`. No original audit
evidence or old archive was overwritten or deleted.

The unchanged kernel was fully rebuilt and saved as
`build/trust-boundary/before.elf`, SHA-256
`4baf8b8e7c1474d1b05d4cb65cd5a540c44a31499e9d75457b8144b78ebd67d1`.
All 23 latest audit assertions were rerun before kernel edits:

- QEMU: **17 cases, 6 passed / 11 failed**, 25 failed assertions, no errors/skips,
  **149.704 seconds**. Includes automatic staged-intent loss on a slot read error.
- Actual-C host: **6 cases, 3 passed / 3 failed**.
- Evidence: `build/trust-boundary/before.json`, `before-qemu/` and `before-host/`.

Original recovery-image SHA-256 at takeover and after these reproductions:

```text
4daf871c5c2982a2c6401a8a3336959f31bdd0143ae00f238f4cbdc7f453f19e
```

## Snapshot placement and replacement authority

Root cause: a maximum historical boot number plus valid sector checksums did
not prove that the intended destination held an old, replaceable record.
Swapping the newest and oldest physical sectors could erase the newest record.

A shared full-history preflight now checks physical `boot % 16` placement,
uniqueness, future/same-boot records, chronology corroboration, and the actual
victim's age (at least 16 boots without underflow/wrap). No sector is repaired
or relocated. Any failed preflight leaves the entire historical area unchanged.

- Added QEMU tests before the fix: **1 passed / 2 failed**, no errors/skips,
  **15.322 seconds**, including a healthy-destination control.
- Focused result after the fix: **21/21 passed**, no errors/skips, **142.314
  seconds**. Includes actual-C placement/age controls, existing future/corrupt
  history and exhaustion cases, corroborated gaps and 18 real boots of rotation.
- Evidence: `build/test-runs/trust-snapshot-before` and `trust-snapshot-fixed`.
- Snapshot encoding, sector ownership and format versions are unchanged.

## Producer-derived event semantics

The shared validator now enforces the event table in `TRUST_INVARIANTS.md`,
reviewed against actual current/baseline producers. It covers scanning, getters,
exports, progression, lifecycle lookup and append itself. Tags remain opaque
printable labels. Failed APPLIED and refused PKG_ROLLED_BACK remain supported;
refused SUGGESTED and inconsistent boot-event facts do not grant authority.

- Before: 6 focused tests, **2 passed / 4 failed**, no errors/skips, **15.944 s**.
- After: **31/31 passed**, no failures/errors/skips, **195.469 s**, including
  F1/F5, real Ring 0 fault persistence and 18-boot rotation.
- Evidence: `build/test-runs/trust-event-before` and `trust-event-fixed`.
- The old positive event/flag cross-product was replaced with producer-valid
  boundaries. It previously asserted that impossible tuples were valid; no
  producer-valid record or refusal evidence was discarded to make tests pass.
- One latest-audit gap fixture assumes its malformed CLEAN_BOOT 17 still raises
  the next boot to 18. Under earlier semantic rejection the correct next boot
  is 17, with healthy replacement of boot 1 only and boot 2 preserved. Rechecks
  must record this stronger expected result rather than preserve poisoned
  ordering merely to satisfy that old intermediate expectation.

## Chronological append authority

The boot scan and replacement proof now share an append-order cursor. Valid
events cannot move backward; duplicate CLEAN_BOOT completion is contradictory.
Gap proof advances an expected boot in physical record order, never by sorting
or a membership set. Every relevant sector is re-read before replacement.
Contradictory chronology holds state/journal persistence at boot while retaining
a diagnostic lower bound. The state writer itself also rejects that authority.

- Before: **1 passed / 2 failed** QEMU cases, **17.139 s**, no errors/skips.
- An intermediate focused run caught recovery-state saves occurring before an
  append-time hold. The hold was moved to boot selection and enforced in the
  state writer as well; that intermediate result is not a passing result.
- Verified focused result: **44/44 passed**, no failures/errors/skips,
  **264.511 s**. Evidence: `build/test-runs/trust-chronology-verified`.
- Actual-C tests cover sector boundaries, post-scan chronology changes,
  duplicate/missing completions, no boot wrap and failed-clear lock retention.
- Confirmed clear releases locks only after all requested zero writes succeed;
  a failed clear cannot manufacture trusted empty chronology.
- Synthetic full-journal fillers now use repeated blocked-canary requests,
  which are legitimate same-boot producer events. Old repeated CLEAN_BOOT filler
  records are not legitimate chronological completion evidence. Preservation,
  fullness and legacy-upgrade assertions retain their original purpose; the
  producer-valid fixture bytes and matching expected prefix were updated.

## Unified package/slot authority

`package_authority.c` is the shared decision boundary. Copy quorum alone is
no longer ownership. The decision combines real slot evidence, manifest binding,
unique current/rollback claims, staged before/after shapes and last acknowledged
durable views. It is used by lifecycle/reconciliation and both state/slot writers.
Candidate coherence is checked together with allowed durable transition edges:
even an internally coherent owner transfer is rejected without its specific
persisted APPLY_STAGED intent. An invalid proposal does not destroy a known
durable current owner.

Module-slot decoding validates actual field values rather than normalizing bad
flags into defaults. Unknown/missing slots cannot authorize active history.
Defaults remain diagnostic only. Explicit first package use writes a positively
pristine factory record in the existing format before staging; cold recovery
therefore reads a real before-side record. No persistent ID/field was needed.

Slot I/O failures latch joint uncertainty and retain stage intent; they do not
publish an assumed abort. Boot reconciliation asks the shared model for the
unique observed result and never manufactures FAILED/ABORT from missing evidence.
Legacy automatic binding was removed. In-session registry reload cannot reset
the latch, even after replacing metadata with coherent stale copies. New boot
trust requires complete joint validation again.

Confirmed reset acquires a scoped RAM-only permission from trusted non-staged
state. It can write only empty package state then the exact factory slot image.
It never resumes after a crash; partial reset leaves uncertainty. There is no
new on-disk reset marker or compatibility fallback.

- Own QEMU regressions before correction: **6/6 failed**, no errors/skips,
  **81.035 s** (`trust-joint-before`).
- First targeted corrected run: **9/9 passed**, **109.936 s**.
- Broader corrected run: **52/52 passed**, no errors/skips, **575.540 s**.
- After transition-edge enforcement: **8/8 actual-C integration checks passed**
  in **1.982 s**, and **25/25 focused checks passed** in **230.268 s**, including
  tagged downgrade/re-upgrade, staged abort/switch and degraded current writes.
- Evidence: `build/test-runs/trust-joint-actual-c`, `trust-joint-broad`,
  `trust-joint-transition-edges` and `trust-joint-verified`.

The older isolated ownership unit fixture is retained for historical evidence;
its two regression purposes now run through the actual persistence/slot engine.
Only ATA outcomes, controlled manifest trust and health/log APIs are mocked in
`joint_authority_fixture.c`. Cold-BSS reset in that fixture is test-only; no
production API clears the sticky authority latch.

### Identity-domain follow-up

The first complete run passed 121 repository cases and both preserved review
corpora. Final boundary review found that state lookup uses case-insensitive
IDs while the new authority comparisons initially used case-sensitive IDs.
A serialized retired `pkg-203` alias before current `PKG-203` was reproduced
as incorrectly trusted (**1 failing actual-C case, 1.159 s**).

Authority uniqueness, durable view lookup and binding identity equality now
use the same case-insensitive domain as the mutator. **19/19 focused checks
passed**, no failures/errors/skips, **111.797 s**. Evidence:
`trust-identity-before` and `trust-identity-fixed`. A final complete run after
this correction is required; the earlier 121-case run is not its acceptance
result. No on-disk field or format changed.

## Final complete results

| Commit | Correction |
|---|---|
| `3b853e4` | Required trust invariants and hash-preserved independent evidence, before kernel edits |
| `207a9de` | Snapshot physical placement, uniqueness and destination age |
| `92a7099` | One producer-derived event-semantic contract |
| `ac29c7b` | Physical append chronology, contiguous proof and persistence holds |
| `fa4ec01` | Joint package/slot authority, durable transition edges, sticky uncertainty and scoped reset |
| `fa4ca0c` | One case-insensitive package principal domain |

Final tested commit: `fa4ca0cec49152b3ef6d6c251171b7652f2a6dfa`.
Git tree: `9c9563a7638d019bc40d5fdc50623bcc9904a783`.
Fresh full-build ELF SHA-256:

```text
9a323f8077b2570004f5150079fce3c017f0ef23543b061588ecf538f8e84f57
```

| Final suite | Exact result | Time |
|---|---|---|
| Repository suite | **122 passed, 0 failures/errors/skips** | **940.245 s** |
| Older preserved QEMU audit | **17 passed, 0 failures/errors/skips** | **148.395 s** |
| Older preserved actual-C audit | **5 passed, 0 failures** | not separately timed |
| Latest QEMU audit replay | **17 passed, 0 failures/errors/skips** | **147.800 s** |
| Latest actual-C audit replay | **6 passed, 0 failures** | not separately timed |

All 93 original repository test IDs are retained; 29 new case IDs were added.
This includes the existing 115 repository/preserved case purposes and the latest
23 audit cases, with the explicit semantic/setup adaptations below. Repeated
runs are not counted as new cases. No original independent evidence was edited.
These are implementing-agent checks, **not independent release acceptance**.

### Exact fixture/replay adaptations

- The unsafe positive event/flag cross-product now tests the reviewed producer
  tuples, including legitimate failure/refusal records. Duplicate CLEAN_BOOT
  synthetic fillers became repeated blocked-canary events; strict history-byte
  preservation/fullness/legacy checks are retained with matching valid inputs.
- N4's two ownership-commit regression purposes now link the real authority,
  persistence, slot and lifecycle engine, rather than approving a mocked quorum.
  T4 adds direct candidate-edge, staged-failure, reset and latch controls.
- The older copy-generation host matrix remains an **isolated copy-layer** test:
  new joint-boundary hooks are mocked only in its scratch fixture. Its assertions
  still verify copy selection, ambiguity locks and generation wrap. Real joint
  permissions are independently exercised by the actual-engine T4 integration.
- Direct new chronology-helper fixtures explicitly establish the new boot-order
  precondition, then test the actual append revalidation. The predicate is not
  replaced or made permissive.
- The latest package host fixture links actual joint/slot code and a matching
  controlled manifest. Its changing no-disk/capacity/read conditions are distinct
  simulated cold environments; a test-only BSS reset expresses that setup. The
  production latch has no reset API. T4 separately tests real in-session reload
  without clearing BSS and requires the hold to remain even with coherent stale
  copies substituted. No test-only code is linked into the kernel.
- One latest QEMU fixture's malformed CLEAN_BOOT 17 no longer advances the boot
  to 18: correct next boot is 17, replacing aged boot 1 only and preserving boot 2.
  That expected result is stronger semantic rejection, not a permissive fallback.
  The other QEMU audit assertions are unchanged (apart from helper-root relocation).
- Host include paths and required diagnostic/capacity providers were ported to
  the new APIs in scratch copies. Initial adapter linking lacked a diagnostic
  stub; it was corrected without changing product code or assertions. Only the
  completed passing replay receipts are counted above.

### Durable transitions observed

| Situation | Result |
|---|---|
| Confirmed pristine first package use | Existing-format factory slot record acknowledged before package staging |
| Valid apply | Durable intent -> acknowledged slot switch -> APPLIED and retirement publication |
| Slot read/validation uncertainty | No reconciliation or state/slot writes; APPLY_STAGED remains byte-identical |
| Slot write returns failure | Hold authority; retain staged intent; do not publish a guessed abort |
| Failed final state save | Restore proposed RAM records, hold authority and retain durable stage |
| Healthy cold read of real before side | Supported abort publication; previous legitimate owner remains usable |
| Healthy cold read of real after side | Publish observed applied/verified/rolled-back result and retire rights as required |
| Same-boot stale registry reload | Hold persists; no publication and no ownership-dependent command writes |
| Explicit healthy reset | Scoped empty-state then exact-factory writes; permission ends immediately |
| Partial reset / unknown outcome | Permission revoked; reboot holds inconsistent evidence rather than guessing completion |
| Contradictory journal order | Diagnostic boot lower bound retained; persistent recovery/history writes held |
| Misplaced/recent/future snapshot victim | Every historical sector remains unchanged |

### Evidence and commands

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1
python -B tests/run_regression.py --no-build --baseline-elf "C:\Users\imran\AppData\Local\Temp\opencode\kmz-release-audit-8e75093\baseline\build\kamikaze.elf" --run-dir build/test-runs/trust-invariants-final-identity
python -B build/trust-boundary/recheck.py --host-only --suffix final
python -B build/trust-boundary/recheck.py --no-host --suffix final
python -B build/trust-boundary/finalize.py
```

- `build/test-runs/trust-invariants-final-identity/summary.txt` and per-case
  images/logs retain complete repository results, including TinyFS, policy,
  host consumers, 18-boot rotation and held privilege controls.
- `build/trust-boundary/recheck-final/old-audit-final/` and `new-audit-final/`
  retain both QEMU replays. `recheck-fixtures/` contains the exact scratch
  adaptations. `host-rechecks.json` and `rechecks.json` identify every adaptation.
- `build/trust-boundary/final.json` records source tree/ELF/image/archive hashes,
  all retained original case IDs and final counts.
- Tagged baseline ELF matched the preserved provenance at `f6c1814`:
  `5c1314ec2cbadf9b3751e5e92510b1dfe851cecf095dc14a39a47090f606a726`.

Original recovery-image SHA-256 and both audit archive checksums matched at
takeover, after all completed runs and at final verification. Ring 3 source and
assembly are unchanged from `8e75093`, approval remains zero, and no CPL3 entry,
merge or push occurred. A final documentation-only commit does not alter the
tested kernel. No temporary audit evidence was cleaned up.

## Compatibility and remaining limitations

- **No storage migration or new persistent field/transaction ID.** Package
  encoder and slot serialization were explicitly compared with `8e75093` and
  remain unchanged. LBA owners, record versions/sizes and checksum coverage are
  unchanged. Durable snapshots, joint state, latch and reset grant exist in RAM.
- Healthy baseline downgrade/re-upgrade and 32/50/51/2048-sector recovery-store
  operation pass. Old kernels retain their weaker policies; readability does
  not make old-kernel mutation commands safe under the new invariants.
- Legacy/unbound, unavailable, contradictory or ambiguous evidence is preserved
  but held. Unsupported chronology can reduce availability, including holding
  snapshot writes indefinitely until separately reviewed intervention. There
  is no automatic repair/rebinding or reset bypass.
- Acknowledged writes are the tested durability model. Physical power-loss,
  torn-sector and write/flush atomicity remain unproven. No atomic reset across
  the package/slot sectors is claimed; interruption deliberately holds.
- This is consistency for fixed metadata-only package updates, not authenticated
  anti-rollback or arbitrary binary-module integrity. Coherent hostile rewriting
  of all relevant metadata is not prevented. Kernel signature checking remains
  simulated. Stronger persistent identities/payload binding would require a
  separately approved format/migration design before any implementation.
- Generation half-range assumptions and 8-bit boot exhaustion holds remain.
  No concurrent external writer, arbitrary kernel-RAM corruption, process
  isolation or new memory-protection guarantee is claimed. Ring 3 remains held.

**Another independent reviewer must verify the corrections and fixture scopes
before release acceptance.**
