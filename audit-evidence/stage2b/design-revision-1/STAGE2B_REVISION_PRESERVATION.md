# Stage 2B Design Revision 1 — preservation and provenance

## Result

**Preservation verified for the declared content/ref scope.** The read-only
`finish` receipt found no changed protected file or Git ref:

| Check | Final observed result |
| --- | --- |
| Original repository/evidence files compared | **6,507** |
| Changed original files | **0** (`changed_original_files=[]`) |
| Historical directories compared | **8**, all unchanged |
| Files within those historical directories | **7,343**, all unchanged |
| All Git refs | Unchanged |
| HEAD / tree / branch | Same identities listed below |
| Working tree | Clean |

See `PRESERVATION_RESULT.json` and the complete before/after content inventories.

Production identity recorded before detailed revision work:

- Repository `${REPOSITORY}`.
- Branch `feature/v2.6.2-memory-protection-phase-b`.
- HEAD `dbf23ce74a2a7103ae6fce04fba51969b7807178`.
- Tree `62ff1f4fbd5d70163c074fb7c259f4f2115c4d77`.
- Working tree clean; all seven authoritative inputs available (`missing_inputs=[]`).

## Protected scope and receipt method

`revision_inventory.py` is a new isolated **inspection-receipt generator**, not a
host test, fault fixture, observer implementation or kernel build script. Its only
operations read/hash declared files, inspect existing ELF/data, use read-only Git,
disassemble P to new text and write revision receipts in this new directory.
It imports/runs no existing test or fixture functions. `seed_reference` inspects
the original seed bytes only; it does not construct even an in-memory conflicting
state sector or a new image.

Before/after production inventories cover:

- Every Git-tracked working file.
- Every file in `audit-evidence/`, `build/test-runs/` and `build/preservation/`,
  including ignored evidence/image/archive data in those trees.
- Original `build/recovery.img` and `build/kamikaze.elf`.

Separately, complete file/content inventories cover eight historical directories
under `${LOCAL_EVIDENCE}`:

1. `kmz-v262-phase-a-review`
2. `kmz-v262-phase-b-review-dbf23ce`
3. `kmz-v262-phase-b-stage1-dbf23ce`
4. `kmz-v262-phase-b-stage2a-dbf23ce`
5. `kmz-v262-phase-b-stage2a-revalidate-dbf23ce`
6. `kmz-v262-stage2a-acceptance-dbf23ce`
7. `kmz-v262-stage2b-design-dbf23ce`
8. `kmz-v262-stage2b-adversarial-review-dbf23ce`

All Git refs are captured/compared, optional locks disabled. The receipt also
checks HEAD/tree/branch/clean state and rechecks authoritative input hashes when
sealing outputs. Preliminary read-only input/identity inspection preceded the
before inventory; no protected write preceded it. Comparison is by content hash,
not access time. No old artifact or failed result is removed or relabelled.

The before receipts are `PRODUCTION_BEFORE.json`, `HISTORICAL_BEFORE.json` and
`REFS_BEFORE.txt`. Final after/comparison receipts are `PRODUCTION_AFTER.json`,
`HISTORICAL_AFTER.json`, `REFS_AFTER.txt` and `PRESERVATION_RESULT.json`. Final
provenance records their hashes and the six Markdown deliverable hashes. The
separate `REVISION_ARTIFACT_MANIFEST.json` hashes provenance and every other new
file, excluding only its own self-hash to avoid a circular digest.

## Critical unchanged baseline identities

The comparison retained these identities; final observed values are recorded in
provenance and the after inventories:

| Artifact | SHA-256 |
| --- | --- |
| Original recovery image | `4daf871c5c2982a2c6401a8a3336959f31bdd0143ae00f238f4cbdc7f453f19e` |
| `release-8e75093.zip` | `f5982850ab1d46a4ca4f0d9bdb09fab31311abc9e66f64387848ae8a10235365` |
| `release-0cd86d8.zip` | `bae909075761af2c3edafe33e993002487d79cc61c9c0dae483ffccb2b75be75` |
| Original Stage2B design | `ae77ff66804a2ed98e8d2f0cab8b1ed15722749872c3ece782c7f72869a5c9fe` |
| Original failed Stage2A result | `70600df8e31af2bfc35235bc03310c913160fd58c5ef59cc39ef68abcca19a1a` |

The failed result was read as `passed=false`, `exit_code=1`, `requires_review=true`;
its initial halt-observation failure stays distinct from later accepted cases.
Existing Stage2A62+4/healthy serial acceptance and all five ELF identities remain
historical reference evidence, never Stage2B execution/calibration results.

The preserved Stage1 seed is read-only SHA-256
`2d1a74227f1ec230a60eb447d534ed5bd67866ee859ca377f64c549c3b3413d6`.
`READ_ONLY_SEED_REFERENCE.json`/`SEED_JOURNAL_INSPECTION.json` report its actual
state/journal bytes and checksums. **No copy was created, attached or mutated.**
The proposed P1-LK copy/hash remains UNRESOLVED—NOT CREATED.

## New output and action scope

All new files are inside:

`${LOCAL_EVIDENCE}\kmz-v262-stage2b-design-revision-1-dbf23ce`

They are the seven requested deliverables, static disassembly/reference-data
receipts, before/after inventories, this directory's inspection script and output
hash manifest. H0's56 controls are **written specifications only**; no executable
control inputs or control runner was created.

| Action in this revision | Count |
| --- | --- |
| Kernel/boot-image/fixture builds | **0** |
| Tests or host-control executions | **0** |
| QEMU launches | **0** |
| CPU faults/device failures injected | **0** |
| Fault fixtures created or activated | **0** |
| Production or existing host-test/observer/source edits | **0** |
| Git checkout/reset/commit/amend/merge/rebase/push | **0** |
| END/Fibi/adaptive-execution imports or implementation | **0** |

Read-only Git/ELF/disassembly/hash/data inspection and documentation/inventory
sealing are not behavioral tests or calibration. Counts describe actions taken
in this revision, not an audit of every process ever run on the host.

## Limits and final design status

The declared content/ref comparison is not a global filesystem, full `.git`
internal-byte or process-history audit. Files outside the enumerated scope are
not asserted unchanged. Statically inspecting historical CPU/I/O traces is not
fresh hardware execution, an RTC coverage attestation or live observer calibration.

F01/F03/F06 are RESOLVED IN DESIGN; F02/F04/F05 are PARTIALLY RESOLVED. U-BUILD,
U-RAM, U-CMOS, U-WRITER, U-DEVICE, U-P2, U-UART and U-P3 remain explicit unresolved
prerequisites. Complete revised documents can be submitted for independent review
while these later gates remain blocked. No new case is accepted or authorized.

**REVISION READY FOR INDEPENDENT REVIEW**
