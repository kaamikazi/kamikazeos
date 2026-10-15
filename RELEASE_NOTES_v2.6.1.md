# KamikazeOS v2.6.1 "Integrity Gate" (hardening, pending re-review)

v2.6.1 fixes the recovery, storage, package, manifest, and health-scoring
defects found by an independent read-only audit of v2.6.0. It adds **no
OS features**. Ring 3 stays held (`execution_approved=no`), nothing repairs
itself, and package and recovery policy is only tightened.

The review of `0cd86d8` returned **REVISE**: its 70 supplied regressions passed,
but eight additional audit cases exposed two remaining blockers. Subsequent
implementation corrections pass **93/93 repository tests** plus re-execution
of all **22 audit checks**. Journal field semantics and snapshot progression
are constrained, and uncertain package redundancy no longer authorizes stale
rollback. No format migration was required. Another independent reviewer must
verify these corrections; see `docs/RELEASE_BLOCKERS.md` and `audit-evidence/`.

The subsequent independent review of `8e75093` also returned **REVISE**. Joint
package/slot authority, durable transition edges, physical snapshot placement,
chronological proof and producer-derived event semantics now address that
review. Final implementation checks pass 122 repository tests plus both audit
corpora (22 and 23 checks), with exact fixture scopes/adaptations documented in
`docs/TRUST_CORRECTIONS.md`. No disk migration was required. This does not
replace independent release acceptance; the required invariants were committed
before implementation in `docs/TRUST_INVARIANTS.md`.

## Findings closed

| # | Finding (reproduced on v2.6.0 `f6c1814`) | Fix |
|---|---|---|
| 1 | A torn journal append was overwritten on the next boot. A damaged state block rewound the journal to slot 0 and overwrote history. | Append position rebuilt from the journal itself, never reusing an occupied slot. Generation + CRC-32 state with a mirror at LBA 50. Read-only store on ambiguity. |
| 2 | `rollback-pkg PKG-201` was accepted after PKG-202 updated the same module, undoing PKG-202. `apply-pkg` overwrote the only verified slot. | Ownership checks before rollback (`PKG-REJECT-001`). No apply over an unverified update (`PKG-REJECT-002`). |
| 3 | `seed-manifests confirm` blocked every built-in package and overwrote an imported PKG-101. | Disk seed copies trusted by exact identity with the compiled-in seeds, never labelled as signed. Seeding fills only empty slots, then package init re-runs. |
| 4 | A clean boot scored 55. Typos cost health and failed `verify-pkg`. Ring wraparound raised health from 30 to 80. An intentional `panic-test` cost 15 points on the next boot. | Health from classified production faults with per-boot counters. Clean boot is 80 (two simulated devices without drivers). |
| 5 | ACT-001's suggestion was journaled every boot. A full journal logged `STORE-WARN-004` five times in one boot. | Suggestions are journaled only on change. A full journal warns once per boot and counts refused appends. |
| 6 | No automated boot or failure tests. | Headless QEMU suite with failure injection on disposable images (`make test`). |

Details: `docs/STORAGE_INTEGRITY.md`, `docs/HEALTH_MODEL.md`,
`docs/TRUST_AND_SIGNATURES.md`, `tests/README.md`.

## Verification

```
python tests/run_regression.py --baseline-elf <v2.6.0 kernel>
```

- Earlier audit records reported 18 failures on v2.6.0 and 33 passing tests
  on the initial v2.6.1 milestone. Those historical counts are not the
  correction branch's acceptance result.
- The takeover baseline executed 32 tests successfully and skipped one
  downgrade case until its baseline ELF was rebuilt. Correction-run counts
  and reproduction evidence are recorded in `docs/INTEGRITY_CORRECTIONS.md`.
- The previous correction suite passed **53/53**, with no failures or skips,
  in 299.100 seconds, including all original tests and tagged downgrade
  compatibility. These are implementation checks, pending independent review.
- The additional audit follow-up independently rebuilt unchanged source and
  passed all 53 baseline tests. Its final expanded suite passed **70/70**, with
  no failures or skips, in **467.440 seconds**. Provenance, separate correction
  commits, reproductions and compatibility are in `docs/INTEGRITY_FOLLOWUP.md`.
- Every QEMU test used its own disposable image. Run directories and evidence
  are recorded in the correction records.
  `build/recovery.img` was hash-checked before and after every run and is
  unchanged (`sha256 4daf871c...`).
- Current regressions execute the analyzer, JSON exporter and headless DOM
  dashboard smoke check. Browser visual QA was not performed in the
  correction run.

## Storage compatibility

No migration. The LBA 1 bytes that v2.6.0 reads keep their meaning. The
extension lives in bytes v2.6.0 always wrote as zero, and the mirror uses
the previously unused LBA 50.

- v2.6.1 boots an image last written by v2.6.0: it upgrades in place on
  the first save (tested).
- v2.6.0 boots an image written by v2.6.1: it loads normally and the boot
  count continues (tested). v2.6.0 drops the extension and the
  intentional-panic flag when it saves, which degrades safely.
- 32/50-sector images use validated single-copy state because LBA 50 is
  absent. At 51 or more sectors the mirror is retained. Existing-sector I/O
  failures lock persistence; they do not silently select compatibility mode.

## Integrity Correction Follow-Up

- D1: capacity-aware state reads/writes and explicit redundancy reporting.
- D2: no snapshot writes while recovery ordering is locked or uncertain;
  readable journal boot bounds remain diagnostic, and snapshot history is
  validated before replacing a ring slot.
- L1: a legacy primary no longer resets a valid mirror generation floor.
- L2: blocked drivers are classified after the final attachment state.
- L3: unexpectedly returning fault triggers clear intentional classification.

These are implementation-tested corrections pending independent re-review.
The Ring 3 gate remains disabled and no v2.6.2 work is included.

### Additional audit follow-up

Four more integrity defects were reproduced and corrected: damaged modern
state misclassified as legacy, unordered generation comparisons (including
package state), inconsistent journal-reader validation, and ABA reuse of stale
package rollback ownership. Existing slot sentinel values support durable
supersession without an incompatible storage-format change or migration.
Ambiguous older ownership fails closed. Independent acceptance remains pending.

## Honesty boundary (unchanged)

- In-kernel Ed25519 verification is still **simulated**. The kernel checks
  host-written metadata. v2.6.1 adds a key-hash consistency check against
  the pinned anchor, which is not cryptography.
- Chained-update tests use fixture manifests carrying the host-verified
  marker. They are written only to disposable test images.
- Excluded health events are still logged, journaled where they were
  before, exported, and shown by the analyzer with a `health=` field.

## Still open (not in this hardening scope)

- The analyzer falls back to the whole multi-boot log if the newest report
  is truncated.
- No header dependency tracking in the Makefile (`build.ps1` always
  rebuilds everything).
- Paging: page 0 mapped, CR0.WP off, no guard pages. Memory above 16 MiB
  is tracked but not mapped.
- `boot_count` and journal boot numbers remain 8-bit; corrected builds hold
  persistent writes at exhaustion rather than reuse boot zero.
- Heap rejections from `heap-alloc` still count as production warnings.
- The approved CPL3 round trip was not re-executed for this release,
  because Ring 3 execution stays disabled. The in-kernel privilege
  self-tests pass on every test boot.
