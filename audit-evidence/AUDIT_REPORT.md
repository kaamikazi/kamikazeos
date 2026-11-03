# KamikazeOS v2.6.1 release verification

## Verdict: REVISE

Target: `hardening/v2.6.1-integrity-gate`,
`0cd86d88a1cc2a57b6a13f8f0e8d003dadf85f59`.
Compatibility baseline: `v2.6.0-pre-audit-baseline`,
`f6c18148e6b003dfe48d3fdded3f83e603d95d2e`.
Read-only audit: original source and HEAD unchanged; no commit, merge, push or
Ring 3 enablement. Everything generated for this audit is in this scratch tree.

The supplied 70/70 claim is reproduced, but additional checks expose two
release-blocking integrity defects. These observations supersede no source
code: this audit does not implement repairs or authorize release acceptance.

## Observed build provenance

`prepare.py` independently exports Git commits, verifies canonical blob hashes
(Git clean filtering accounts for Windows CRLF), enumerates C/assembly sources,
invokes Clang for every translation unit and links with LLD. It does not invoke
the implementation's build script or reuse implementation ELF files.

| Build | Commit | Verified blobs | Translation units | ELF SHA-256 |
|---|---|---:|---:|---|
| Target | `0cd86d8` | 250 | 45 | `b9d9252544a5f908b0d0ce6cc05e939e95f71d4587270b7ddde7fc4bbfd3644a` |
| Tagged baseline | `f6c1814` | 225 | 44 | `c9dd155dcaadeb8762dce76565c9a409a13f0f85a902a69db869227deee17da7` |
| Pre-additional-correction control | `7ccb4d9` | 245 | 45 | `377f3c71c5a462c76c093ba6581f3a2ed5f6f26e192124324f0e5ca7c499980c` |

Toolchain: Clang/LLD 22.1.7, QEMU 10.2.0, Python 3.10.11, Node.js 24.16.0.
Scratch build paths affect debug information, so matching the implementation's
ELF hash is not the provenance criterion. Every exported source blob was
reverified after the tests, and all three exported source trees remain exact.

## Exact final test totals

| Suite | Cases | Passed | Failed cases | Errors | Skips |
|---|---:|---:|---:|---:|---:|
| Supplied regressions | 70 | 70 | 0 | 0 | 0 |
| New QEMU adversarial checks | 17 | 10 | 7 | 0 | 0 |
| New actual-C host checks | 5 | 4 | 1 | 0 | 0 |
| Distinct target cases | **92** | **84** | **8** | **0** | **0** |

The supplied suite took **474.294 seconds**. The main 15-case adversarial run
took **122.161 seconds**; two additional corrupted-copy cases took **28.256
seconds**. The seven failing QEMU cases produce 20 failing subtest assertions;
those assertions are not counted as 20 separate test cases. The host semantic
validation check supplies the eighth failed case.

Two negative controls against `7ccb4d9` reproduced the damaged-extension and
ordinary ABA defects (two expected failures, 13.808 seconds); these are separate
from the 92 target cases. Repeated runs are not double-counted.

Initial scratch write-fault assertions assumed a serial error and omitted
blkdebug's `iotype` filter. They were corrected to use explicit read/write-only
faults and verify real package/slot outcomes. A scratch parser also initially
assumed every package-section row had an ID; this was corrected. Final runs
above have no harness errors. Earlier attempts are retained, not presented as
kernel failures or included in the final distinct-case totals.

## Target findings

### 1. Legacy-state detection: PASS

- Damaged-extension reproducer selects `legacy_primary` on `7ccb4d9`; target
  selects the valid mirror and reports boot 2 rather than the injected boot 78.
- Actual-C fixture checks every reserved byte 18-511 individually (494 cases),
  a genuine unusual legacy record, and every one-byte flip in modern state
  bytes 0-51 (52 cases). All pass.
- QEMU accepts a zero-tail legacy record with panic, safe-mode and quarantine
  flags, reserved journal slots and a max-generation mirror floor. Wrap and
  reserved-slot preservation pass.
- Partial modern corruption on a 32-sector single-copy disk is not hydrated
  as legacy; journal boot bounds and capacity are preserved.

Limit: a modern extension completely replaced with the exact legacy zero tail
is indistinguishable from a genuine legacy write. This is inherent in the
backward-compatible format, not something the zero-tail test authenticates.

### 2. Generation ordering: PASS (arithmetic/conflict handling)

- Actual RecoveryStore and package-state source each pass an independent
  9-by-9 generation matrix: 0, 1, 2, half-range neighbors and maximum values.
- Equality/identical records, equal-generation conflicting records, rollover,
  reversed copy placement and half-range separation are checked. Nine extra
  equal-generation conflict cases are tested per store.
- Package save at `0xffffffff` wraps to zero, persists both copies and reloads.
- QEMU half-range separation near maximum generation preserves state, journal
  and snapshots even after an attempted recovery action.
- Existing package ambiguity regression confirms lifecycle/reset operations
  cannot modify its package/slot sectors when the generation lock is active.

This does not certify safe ordering when a package copy is unavailable. The
new ownership finding below exposes that separate fallback path.

### 3. Journal validation: FAIL (semantic-validation gap)

The original reader inconsistency is corrected: actual scanning, getter and
latest-action lookup agree on magic, version, checksum and event range. Extra
unsupported versions, checksum faults, unknown events, sparse occupied slots,
sector boundaries and a malformed final slot all pass preservation checks.

However, `kernel/src/recovery_store.c:150-156` never validates the result,
risk or reversible field domains. The documented result is 0/1; risk is the
fixed low/medium/high enum; reversible is boolean. Three QEMU fixtures each set
one field to 255 in a checksum-valid SUGGESTED record carrying boot 240.

Observed independently in all three cases:

1. The malformed record is treated as valid and exported as `BADSEM`.
2. Boot count advances from 1 to **241**, not 2.
3. A legitimate suggestion after rollback is suppressed.
4. The boot-1 snapshot at LBA 17 is replaced on the second actual boot, because
   `241 % 16 == 1`. Before/after snapshot sectors are retained in each fixture.

The source chain is validator -> scan max boot (`recovery_store.c:250-256`) ->
trusted boot ordering (`recovery_memory.c:191-202`) -> ring-slot write
(`boot_snapshot.c:194-196`). Getters and lifecycle lookup also share the
incomplete validator (`recovery_store.c:594` and `:644`).

These fixtures deliberately reseal the additive checksum to isolate semantic
validation. This finding is not a claim that checksums authenticate hostile
disk modifications. It demonstrates that impossible field values are accepted
as policy/ordering evidence and can prematurely replace snapshot history.

Required correction: validate supported field domains consistently before a
record contributes ordering, reports or lifecycle decisions; retain invalid
occupied bytes and protect existing snapshot history. Add the three fixtures
to the release regressions.

### 4. Durable rollback ownership: FAIL (stale-copy fallback)

Healthy-state ABA is fixed: the cycle PKG-201 `0.3->0.4`, PKG-202 `0.4->0.3`,
PKG-203 `0.3->0.4` rejects PKG-201, and the current PKG-203 owner can roll back.
Registry reload, reapproval, restarts, staged abort/reconciliation, actual slot
write failures, and actual package staging write failures pass the tested
controls. Supplied host tests also exercise final commit/reconciliation failure.

The retirement marker lives only in package-state copies. A newer copy can
contain the retirements while an older valid copy still grants PKG-201 rights.
`package_state.c:235-238` turns a read failure into zeros; `:260-263` then loads
the remaining valid copy without an ordering lock. A bad-checksum copy similarly
falls out of selection. Slot metadata contains no apply identity to disprove
the stale claim once versions and slot positions repeat.

Four QEMU cases reproduce this, with no target source modifications:

- Older valid primary plus newer mirror with read-only I/O failure.
- Older valid mirror plus newer primary with read-only I/O failure.
- Older valid primary plus newer mirror with a damaged base checksum.
- Older valid mirror plus newer primary with a damaged base checksum.

The stale sector is a saved genuine state from after PKG-201's verified apply;
it models the older copy that degraded mirror writes may leave. The new copy
is genuine state after PKG-203. In every case:

```text
rollback-pkg PKG-201
code=PKG-RB-001 ... Rolled back package PKG-201
active B / version 0.4 / rollback A
  -> active A / version 0.3 / rollback none
```

Thus unavailable newer retirement evidence resurrects a stale owner and
undoes PKG-203. Journal prefixes remain intact, but active slot metadata changes.

Required correction: unavailable/damaged package redundancy must not authorize
ownership from a potentially stale fallback. Conservatively hold lifecycle
transitions where ownership cannot be proven, or design an independently
validated association between slot metadata and a specific apply. Any proposed
incompatible format change requires explicit migration approval before coding.

## Storage compatibility and preservation

- No incompatible storage-format change observed. Legacy base offsets, package
  record sizes, transaction sidecars and slot fields remain compatible. The
  SLOT_NONE value existed before the ownership change.
- Tagged downgrade/re-upgrade passes in both supplied cases; package and slot
  sectors stay byte-identical during the tested old-kernel boot.
- 32/50-sector single-copy and 51/2048-sector mirrored recovery-store tests pass.
  TinyFS/packages still require their own LBA ranges on larger images.
- Journal preservation passes for malformed/full/sparse occupied records and
  tested store locks. Snapshot preservation passes for locks and ordinary
  progression, but fails for the newly discovered semantic-journal poisoning.
- Source Ring 3 approval stays zero in all builds. Supplied held-execution and
  privilege checks pass, and every new QEMU report asserts execution disabled.
- Original recovery image SHA-256, before and after:

```text
4daf871c5c2982a2c6401a8a3336959f31bdd0143ae00f238f4cbdc7f453f19e
```

Original worktree remains clean at the pinned HEAD. All QEMU images are scratch
disposables; no original image was attached to QEMU.

## Remaining risks and unverified properties

- Release blockers: snapshot evidence poisoning and stale rollback authority
  after loss of the newer package-state copy, as observed above.
- Physical torn-sector/power-loss atomicity remains UNVERIFIED. Actual guest
  read/write failures and host state-save failures are not that proof.
- Checksums/CRC are not authentication; kernel signature verification remains
  simulated. Older kernels retain older integrity policy defects.
- Serial generations assume a gap smaller than half the 32-bit range. Boot
  numbers remain 8-bit with the existing exhaustion hold.
- Ambiguous old ownership is refused in healthy readable redundant state, but
  the unavailable-copy path bypasses that protection.
- Current paging protections remain limited (page zero mapped, CR0.WP off,
  no guarded kernel stack); these are outside the Integrity Gate correction.

## Evidence and rerun commands

`provenance.json`, build logs, `audit-evidence.json`, `host-results.json`,
`supplied-70/summary.txt`, `adversarial-final/summary.txt`,
`adversarial-corrupt-confirm/summary.txt`, and
`reproductions-pre-additional/summary.txt` are in this scratch directory.

```powershell
python -B target/tests/run_regression.py --no-build --elf target/build/kamikaze.elf --baseline-elf baseline/build/kamikaze.elf --run-dir <new-scratch-run>
python -B adversarial.py --run-dir <new-scratch-run>
python -B run_host.py
```

The second command now includes all 17 added QEMU cases. Scratch test directories
must be new so previous evidence is not overwritten. The verdict is REVISE;
the conditional v2.6.2 implementation plan is therefore not issued.
