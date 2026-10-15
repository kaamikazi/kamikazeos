# v2.6.1 Additional Integrity Corrections

Implementation evidence, not independent acceptance. Work started 2026-10-15
on `hardening/v2.6.1-integrity-gate`, with a clean worktree at
`7ccb4d9b32941d6f850a274c07e6ad0148bd51cb`.

## Fresh unchanged-source baseline

- Full rebuild using `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1`.
- Current ELF SHA-256: `1193c354337b96e342542d99803f7ad84600d4059b019a84953cef5a585da91a`.
- Compatibility source: freshly exported annotated tag
  `v2.6.0-pre-audit-baseline`, resolving to
  `f6c18148e6b003dfe48d3fdded3f83e603d95d2e`.
- All 225 exported source blobs matched tagged Git objects using Git's clean
  filter (`core.autocrlf=true`). The first raw-byte comparison stopped before
  building because the archive contained Windows line endings; canonical blob
  comparison resolved that verification-method issue.
- Freshly rebuilt compatibility ELF SHA-256:
  `ad05c568f49fa1e6cd685fd2e88dd64605cedb8ed33f7d60e51631fb78586a25`.
- All **53 existing tests passed**, no failures or skips, in **293.312 seconds**.
- Evidence: `build/integrity-20261015/provenance.json` and
  `build/integrity-20261015/regression/summary.txt`.

Original `build/recovery.img` SHA-256 before and after the baseline:

```text
4daf871c5c2982a2c6401a8a3336959f31bdd0143ae00f238f4cbdc7f453f19e
```

The original image is only read for hashing. QEMU uses disposable images.
Both current and tagged sources keep the Ring 3 approval macro at zero.

## Additional corrections

### N1: legacy-state misclassification

- Before: two tests produced five failing assertions (each of four damaged
  extension magic bytes, plus a nonzero reserved tail). The unchanged kernel
  chose `legacy_primary` over a valid mirror.
- Correction: require the complete zero reserved tail written by actual
  legacy kernels. Modern damaged state cannot bypass CRC validation.
- After: **16 tests passed**, no failures/skips, in **96.764 seconds**,
  including N1, F1, L1, all small disks and tagged downgrade/re-upgrade.
- Evidence: `build/test-runs/followup-n1-before` and `followup-n1-fixed`.
- No format change. Genuine legacy state and mirror generation floors remain
  compatible. Existing journal prefixes are preserved.

### N2: ambiguous generation ordering

- Before: five tests ran; the two normal/wrap controls passed, while three
  ambiguity tests produced five failing assertions (both half-range copy
  orientations for recovery/package state, plus equal-generation differing
  package sectors).
- Correction: shared unsigned four-way generation comparison. Recovery locks
  state/journal/snapshot writes on unordered generations. Package state is not
  hydrated on ambiguity and persistence/transition creation is held. A failed
  state reset cannot independently clear module slots.
- After: **25 tests passed**, no failures/skips, in **202.454 seconds**, covering
  N2, F1/F2, D2, L1 and tagged downgrade/re-upgrade.
- Evidence: `build/test-runs/followup-n2-before` and `followup-n2-fixed`.
- No format change. `0xffffffff` to zero remains a valid ordered wrap.

### N3: inconsistent journal validation

- Before: two tests produced three failures: unknown events appeared in the
  journal report, an unsupported-version suggestion was exported, and that
  invalid suggestion suppressed a legitimate new suggestion after rollback.
- Correction: one entry validator for scanning, individual reads and latest
  action lookup. The host decoder now checks the same version/event bounds.
  Occupancy remains independent of validity; no occupied slot is reused.
- After: **21 tests passed**, no failures/skips, in **121.895 seconds**, covering
  N3, F1, F5, R1 and tagged downgrade/re-upgrade (including host report consumers).
- Evidence: `build/test-runs/followup-n3-before-confirm` and `followup-n3-fixed`.
- No format change. Malformed entry bytes and all preceding history remain
  unchanged; unsupported records cannot influence trusted journal consumers.

### N4: stale rollback ownership (ABA)

- Before: the cycle `0.3 -> 0.4 -> 0.3 -> 0.4` accepted PKG-201's stale
  rollback of PKG-203, live and after restart. Four of five QEMU cases failed;
  the staged-abort control passed. Initial staged-log assertions were corrected
  to the existing 15-character error-code limit before this confirmed run.
- Compatibility decision: **no incompatible storage change is necessary**.
  The v1 package record already allows `SLOT_NONE` for `slot_before_apply`.
  New apply commits retire older same-module rights with that value while
  retaining their statuses, version history and manifest identities.
- New-owner publication and retirement share one mirrored state save. Failure
  restores all RAM records and keeps staged intent; unresolved transactions
  hold further lifecycle commands for that module. Existing reconciliation
  retires rights only after a slot switch, preserving them on an aborted apply.
- Older images with indistinguishable live claims refuse both rollbacks rather
  than invent ownership. A successful rollback never resurrects old rights.
- Focused QEMU coverage: **19 tests passed**, no failures/skips, in **149.222
  seconds** (N4, F2/F3/F4). Additional compatibility/failure-boundary coverage:
  **3 passed**, no failures/skips, in **18.098 seconds**.
- The actual pre-N4 C (`90ee172`) also failed both ownership host fixture
  exports; the corrected C passed. Fixtures cover successful multi-record
  retirement, failed owner commit, failed reconciliation and successful retry,
  including preservation of unrelated-module rights.
- Tagged old-kernel boot reads the new records and leaves package/slot sectors
  byte-identical; re-upgrade retains the new exclusive rollback authority.
- Evidence: `build/test-runs/followup-n4-before-confirm`, `followup-n4-fixed`,
  `followup-n4-boundaries`, and `build/integrity-20261015/ownership-before`.

## Final verification

All four corrections were committed separately:

| Commit | Correction |
|---|---|
| `f4281b1` | Reject damaged modern state masquerading as legacy |
| `ee25ad0` | Hold persistence when state generations are unordered |
| `90ee172` | Validate journal entries consistently across all readers |
| `d91eba4` | Retire superseded package rollback rights durably |

Final tested source: `d91eba47ff4d450efff81cade68dfbc0275cfbd8`.
The full build and regression command was:

```powershell
python tests/run_regression.py --baseline-elf build/integrity-20261015/tagged-source/build/kamikaze.elf --run-dir build/test-runs/followup-final
```

Result: **70 tests passed, 0 failures, 0 errors, 0 skips, in 467.440 seconds**.
This includes all 53 unchanged original tests plus 17 additional tests.
The runner exited zero. Evidence: `build/test-runs/followup-final/summary.txt`
and its per-test images, serial logs and host-fixture outputs.

Final kernel ELF SHA-256:

```text
587709147dad6d91894d3cd2fc29cef0ef9bf9f0103e9e401579bf6781fdbf79
```

The original recovery-image SHA-256 remained
`4daf871c5c2982a2c6401a8a3336959f31bdd0143ae00f238f4cbdc7f453f19e`
through every completed test run and the final explicit hash check.
Ring 3 code and assembly have no changes relative to the starting commit;
the held-execution regression passes. No CPL3 transition, merge or push occurred.

## Storage compatibility and remaining limitations

- No incompatible storage change or migration. Existing LBA ownership, v1
  fields, extension layouts, record sizes, event numbers and checksum coverage
  remain unchanged. Rollback retirement uses an existing `SLOT_NONE` value.
- Genuine legacy upgrades, tagged downgrade/re-upgrade, 32/50-sector single-copy
  disks and 51/2048-sector mirrored disks pass. The extra package compatibility
  test confirms old-kernel boot leaves corrected package/slot sectors unchanged
  and re-upgrade retains exclusive rollback rights.
- Old kernels retain their historical integrity-policy defects. Compatibility
  proves readability and the tested round trips, not safe execution of every
  old-kernel package command.
- Ambiguous older ownership cannot be reconstructed uniquely from existing
  version/slot metadata. Competing rollbacks remain refused; no automatic
  repair or guessed migration is performed.
- Physical power interruption during an ATA write/flush or a torn containing
  journal sector is not proven safe by these tests. QEMU cases inject between-
  boot state and read errors; host ownership fixtures model save outcomes,
  rather than physical media atomicity. Package checksums remain additive;
  integrity checks are not cryptographic authentication.
- Generation comparison assumes successive valid generations are separated
  by less than half the 32-bit range; exact half-range separation is held as
  ambiguous. Boot numbers remain 8-bit with the existing exhaustion hold.
- Kernel signature verification remains simulated. Browser visual QA and new
  live Ring 3 execution are not claimed.

These are implementing-agent regression results. **Independent acceptance
remains pending.**
