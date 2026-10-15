# v2.6.1 architectural trust-boundary corrections

Implementation evidence, not independent release acceptance. Starting clean
HEAD: `8e75093f1d880d8b546d4f0c8f0d6e7ef9506086`, branch
`hardening/v2.6.1-integrity-gate`. Required invariants were written and committed
as `3b853e4` **before kernel implementation**; see `TRUST_INVARIANTS.md`.

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
