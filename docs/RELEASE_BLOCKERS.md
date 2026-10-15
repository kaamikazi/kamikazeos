# v2.6.1 release-blocker corrections

Implementation checks, **not independent release acceptance**. Another reviewer
must verify these corrections. Starting branch:
`hardening/v2.6.1-integrity-gate`, starting HEAD:
`0cd86d88a1cc2a57b6a13f8f0e8d003dadf85f59` (clean tracked source).

The subsequent review of `8e75093` returned REVISE. Its preserved evidence is
under `audit-evidence/8e75093/`. Later architectural corrections and final
122-test results are in `TRUST_INVARIANTS.md` and `TRUST_CORRECTIONS.md`; the
historical results below are not independent acceptance.
Starting Git tree hash: `4f2fe3bcbcc06865a73f88c793ada03a13271568`.

## Preserved evidence and unchanged-source reproduction

Read the independent report and adversarial results before changing kernel
source. The full independent audit was archived outside temporary/build storage;
see `audit-evidence/README.md` for SHA-256, inventory and readable report copies.

The unchanged kernel was fully rebuilt; its ELF SHA-256 matched the existing
artifact: `587709147dad6d91894d3cd2fc29cef0ef9bf9f0103e9e401579bf6781fdbf79`.
The pre-correction ELF is retained at `build/revise-blockers/before.elf`.
Original recovery image SHA-256 at takeover:

```text
4daf871c5c2982a2c6401a8a3336959f31bdd0143ae00f238f4cbdc7f453f19e
```

- Reran the seven failing QEMU cases from the original independent audit:
  **7 failed cases, 20 failing subtest assertions, no errors/skips**, in
  **77.317 seconds**. Malformed boot-240 entries replaced old snapshots;
  primary/mirror read and checksum failures resurrected PKG-201 ownership.
- Added repository regressions before fixes: **13 tests: 3 passed, 10 failed
  cases, 22 failing assertions, no errors/skips**, in **159.096 seconds**.
  Controls include valid field boundaries, a complete clean-boot chain and
  healthy 18-boot snapshot rotation.
- Evidence: `build/revise-blockers/before.json`,
  `build/revise-blockers/audit-reproduced-before/`, and
  `build/test-runs/revise-regressions-before/`.
- All tests use disposable images. The original recovery image hash was
  unchanged after both reproduction runs. Ring 3 remains held.

## Blocker 1: malformed journal evidence and snapshot progression

The single shared validator now enforces version/event/checksum plus 0/1
result and reversible flags, the supported risk enum, fixed action ID domain,
zero reserved byte, and printable, zero-padded six-byte tags. Package events
retain the complete package-byte range. Boot numbers retain their existing
8-bit representation. Occupancy remains separate: rejected entries are still
reserved and preserved byte-for-byte, but cannot affect ordering or consumers.

Snapshot writes require consecutive historical progression or domain-valid,
successful CLEAN_BOOT evidence for every intervening boot in a gap. A lone
high boot number remains diagnostic ordering evidence, not proof that a
16-slot ring aged enough to replace snapshots. Full historical preflight and
existing ambiguity/exhaustion locks remain in force. The check is bounded to
the existing journal and reads each containing sector once.

These are consistency checks, **not authentication**. A party able to rewrite
all mutually consistent metadata and checksums is not excluded by this format.
Uncorroborated gaps hold snapshot writes; healthy consecutive rotation and
complete clean-boot chains remain supported.

Focused build/test result: **32 passed, 0 failures/errors/skips, in 268.974
seconds**. Includes R5, F1, D2, N2, N3 and tagged downgrade/re-upgrade.
Evidence: `build/test-runs/revise-journal-fixed/summary.txt`.
No on-disk layout, checksum coverage, record size or version changed.

## Blocker 2: stale package rollback after copy failure

At boot, both package-state copies must be readable, valid and unambiguously
ordered before ownership-dependent mutations are authorized. Read failures no
longer become empty copies; checksum/sidecar failures no longer authorize an
older fallback. Empty package sectors beside existing module-slot evidence
are not a fresh store. Unbound legacy data remains preserved without automatic
binding or upgrade. Initial empty state/slot sectors still support normal use.

Runtime/report ownership states are `fresh`, `verified`, `degraded_current`,
`uncertain` and `unavailable`. Uncertain state does not hydrate package records,
so it cannot trigger automatic transaction reconciliation or legacy migration.
Persistence gates also hold ensure/save/reset; the lifecycle gate emits
`PKG-REJECT-003` and returns failure before a staged or slot mutation. Reset
cannot clear slots if its package-state clear was refused.

A known successful current one-copy write can complete a requested transaction
in that session (`degraded_current`). This is distinct from guessing ownership
from a fallback at boot. Both write failures hold later operations; reboots
revalidate redundancy. No questionable copy is automatically repaired. A
transient read failure can be cleared externally and followed by a healthy
reboot; valid current ownership then permits normal rollback.

Focused result: **30 passed, 0 failures/errors/skips, in 433.609 seconds**,
including R6, N4, F2, N2 and tagged downgrade/re-upgrade. After adding structured
refusal logging, **6 passed**, no failures/errors/skips, in **74.483 seconds**.
Evidence: `build/test-runs/revise-ownership-fixed` and
`build/test-runs/revise-ownership-refusal-log`.

R6 additionally covers missing peers in both directions, damaged sidecar magic
and checksum, erased state with active slots, legacy holds, a real interrupted
first state write, degraded current applies, fault-clear reboot, and staged
intent beside a bad peer. Uncertain package/slot sectors are compared byte-for-
byte after lifecycle/reset attempts; existing journal prefixes remain intact.

On-disk compatibility is preserved; no migration was required. Normal rollback
and existing readable/ordered staged recovery remain supported. Old unbound or
incomplete package metadata is intentionally held for investigation instead of
being automatically upgraded; this is a stricter authorization policy, not a
new disk format. Independent acceptance remains pending.

## Final complete verification

| Commit | Scope |
|---|---|
| `463c3a9` | Journal field domains, corroborated snapshot progression, failing regressions and preserved independent evidence |
| `eee0030` | Explicit package ownership trust/holds, no stale fallback authority, failure/restart/compatibility tests |

Final tested source commit: `eee00309885c18cfe13ecdc80e9b1c28e09c40b7`.
Its Git tree hash is `fdc3c154fa5fdf117ef1d6040aec2e58449e01b6`.
The final kernel was fully rebuilt with `scripts/build.ps1`; ELF SHA-256:

```text
4baf8b8e7c1474d1b05d4cb65cd5a540c44a31499e9d75457b8144b78ebd67d1
```

| Final suite | Result | Time |
|---|---|---|
| Complete repository suite (70 existing + 23 new regressions) | **93 passed, 0 failures/errors/skips** | **796.417 seconds** |
| Preserved independent-audit QEMU assertions, run on corrected ELF | **17 passed, 0 failures/errors/skips** | **147.134 seconds** |
| Preserved audit actual-C host assertions, run on corrected C | **5 passed, 0 failures** | not separately timed |

The eight previously failing adversarial cases now pass. Re-execution of another
reviewer's assertions by the implementing agent is **not a new independent
review or release acceptance**.

The preserved host fixtures were copied to a new build directory; only include
paths were redirected to current kernel source, and the package fixture gained
a `ata_capacity_sectors() == 64` mock required by the new capacity check. Audit
assertions were unchanged. The original audit source, reports and archive were
not edited. The QEMU adversarial script itself was run without changes.

The tagged baseline ELF SHA-256 matched preserved independent provenance:
`c9dd155dcaadeb8762dce76565c9a409a13f0f85a902a69db869227deee17da7`
at `f6c18148e6b003dfe48d3fdded3f83e603d95d2e`.

Commands and evidence:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1
python -B tests/run_regression.py --no-build --baseline-elf "C:\Users\imran\AppData\Local\Temp\opencode\kmz-release-audit-0cd86d8\baseline\build\kamikaze.elf" --run-dir build/test-runs/revise-blockers-final
python -B build/revise-blockers/recheck_audit.py
python -B build/revise-blockers/finalize.py
```

- `build/test-runs/revise-blockers-final/summary.txt`: complete suite and
  per-test disposable media, serial logs, analyzer/dashboard outputs.
- `build/revise-blockers/current-audit-qemu/summary.txt`: all 17 audit cases.
- `build/revise-blockers/current-audit-host/`: redirected host fixture source,
  compiler output and WebAssembly artifacts.
- `build/revise-blockers/audit-recheck.json` and `final.json`: exact results,
  source commit, ELF hash and before/after recovery hashes.

The original recovery image SHA-256 matched takeover after every completed run
and the final explicit check. The preserved audit ZIP checksum also still
matched. No original image was attached to QEMU, reset or deleted. Ring 3 source
and assembly are unchanged from `0cd86d8`; the build-time approval remains zero
and held-execution tests pass. No merge or push occurred.

## Compatibility and limitations

- No migration was required. Existing LBA owners, field offsets, format versions,
  checksum coverage and record sizes are unchanged. The package-sector encoder
  was explicitly compared with `0cd86d8` and is byte-for-byte unchanged in source.
  New ownership state/reason/permission fields exist only in RAM and reports.
- Full suite passes small-disk operation at 32/50/51/2048 sectors, tagged recovery
  downgrade/re-upgrade, and package retirement across tagged downgrade/re-upgrade.
- Legacy package records and incomplete redundancy are preserved but intentionally
  held rather than automatically promoted to ownership authority. Persistent
  uncertainty requires investigation; no automatic repair/reset bypass is added.
- Consecutive snapshot rotation and complete clean-boot chains pass. A gap lacking
  corroboration intentionally holds further snapshot writes. This favors retained
  evidence over automatic history availability.
- Physical power-loss/torn-sector/flush atomicity remains unproven. Tested guest
  read/write errors and mocked host save failures do not establish that property.
- Checksums/CRC are consistency checks, not authentication. Coherent rollback or
  hostile rewriting of all metadata is not prevented. Kernel signature checking
  remains simulated; older kernels retain older policy defects.
- Serial generations retain the half-range assumption; boot numbers remain 8-bit
  with the existing exhaustion hold. No memory-protection or CPL3 work is included.

**Another reviewer must independently verify both corrections before acceptance.**
