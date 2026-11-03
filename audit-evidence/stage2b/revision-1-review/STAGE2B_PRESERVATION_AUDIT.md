# Independent Preservation Audit

Scope: actual file hashes, supplied inventories, Git identities/ref maps,
existing ELF and preserved data. No build/test/control/fixture/VM was executed.
This audit does not reconstruct every historical host process or causally
attribute missing files/ref differences.

## Revision Package Identity

All21 named artifact-manifest entries match their actual SHA-256 values. The
manifest excludes its own hash; independent digest is:

```text
9c3ebe13c108e0f837005cb2a34a7e3b3a8efd4206defdd1edb043a9a17c396b
```

No extra revision file was found. Complete required Markdown, provenance,
preservation inventories, disassembly/binding/CMOS/seed receipts and the
revision's receipt-generator source were inspected. The inventory generator
was read as text and was not executed. Its allowed operations are file reads/
hashes, Git reads and disassembly into its own directory; its zero-execution
claim remains an author action record rather than a universal process audit.

Seven authoritative original design/review-input hashes match. The author's
PRODUCTION_BEFORE/AFTER inventories are equal, as are HISTORICAL_BEFORE/AFTER
and REFS_BEFORE/AFTER. This independently establishes internal receipt consistency;
the current-file comparisons below determine what can be corroborated today.

## Production Baseline: Corroborated

| Property | Current independent observation |
|---|---|
| HEAD | dbf23ce74a2a7103ae6fce04fba51969b7807178 |
| Tree | 62ff1f4fbd5d70163c074fb7c259f4f2115c4d77 |
| Branch | feature/v2.6.2-memory-protection-phase-b |
| Working status | Clean |
| Enumerated production/evidence paths |6507 checked;0 missing,0 changed,0 added in the declared scope |
| Production ELF |7cc1262fb1caee2b104d17d601a19e172eb02eac6d93b56ad2b2612a228db7bc |
| Original recovery image |4daf871c5c2982a2c6401a8a3336959f31bdd0143ae00f238f4cbdc7f453f19e |

The scope follows the revision: tracked working files, audit-evidence,
build/test-runs, build/preservation, original recovery image and production ELF.
It does not claim every ignored production-directory file or Git object byte
was audited. Current worktree/source state is untouched by this review.

## Historical Scope: Qualification Required

| Historical directory suffix | Expected paths | Missing now | Changed surviving |
|---|---:|---:|---:|
| kmz-v262-phase-a-review |1285|10|0|
| kmz-v262-phase-b-review-dbf23ce |1368|16|0|
| kmz-v262-phase-b-stage1-dbf23ce |2536|38|0|
| kmz-v262-phase-b-stage2a-dbf23ce |463|0|0|
| kmz-v262-phase-b-stage2a-revalidate-dbf23ce |822|0|0|
| kmz-v262-stage2a-acceptance-dbf23ce |829|0|0|
| kmz-v262-stage2b-design-dbf23ce |21|0|0|
| kmz-v262-stage2b-adversarial-review-dbf23ce |19|0|0|
| Total |7343|64|0|

All64 absent paths had the recorded empty-file digest
`e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`.
They include compile logs, worker/oracle logs, excluded-tests files and zero-byte
CPU trace placeholders. No nonempty historical content was found changed or
missing in this inventory, and no historical additions were found. Nevertheless,
an absent empty path is not an unchanged file: the full current7343-path equality
claim is not established.7279 extant paths match their expected content.

The exact64 paths and recorded hashes are retained in
`PRESERVATION_QUALIFICATIONS.json`. No path was recreated, restored or deleted.
This evidence cannot show whether an external cleanup occurred before this
review, whether the author saw the files on2026-10-23, or who changed existence.
Do not infer author misconduct or retrofit the original inventory.

## Git Refs: Current Map Differs At Internal Refs

Branch/tag refs match the revision's map. HEAD/tree/current branch match and
the worktree is clean. The complete map is not identical:

- Current-only: `refs/codex/turn-diffs/captures/1793248237470/6b4935b3-8650-457c-8a00-247b1da492ee/base`
  -> tree62ff1f4fbd5d70163c074fb7c259f4f2115c4d77.
- Revision-only: `refs/codex/turn-diffs/checkpoints/4857c41bac1b2ab76f5dedd4b5d1a35f/d327ba33e11f872c9fd7f268cdb0edd2/1792149965400/a87f2024-332c-4f90-85be-b5b517c13ccd`
  -> c7dd73bba1c1868c2b1b8b0707e4adfbc3f3195c.

Their names identify internal Codex ref namespaces. Cause/timing is not
established. This reviewer issued only read-only Git commands, with optional
locks disabled for inventory/status. No ref was rewritten to force equality.
The author's own before/after map equality remains an internally consistent
historical claim; current all-ref equality must be qualified.

## Stage 2A Evidence Preserved In Its Original Scope

All three relevant Stage 2A trees match their complete declared inventories.
Five existing ELF digests match revision provenance, including:

- Original Stage 2A ae7d6975f5156581fc1af0f346f7f4e1b69062ba49971af371dd80d5861e9e66.
- Revalidated55697ce3bd4336a201da7ae93f3cabbe714e142abb84e4c3bd697e0d367a4133.
- Accepted79415990686f9ab0579c6f86f216e1fd7d372965eb7e7bf2f0e7e952ffecdd50.

Original failed `case1/result.json` remains passed=false, exit1, with
`No intended DF terminal halt before deadline` and SHA-256
`70600df8e31af2bfc35235bc03310c913160fd58c5ef59cc39ef68abcca19a1a`.
No report was rewritten to repair that result.

Retained accepted case1/case2 activation, CPU registers, trace windows, images
and result/acceptance receipts corroborate their bounded historical evidence.
Both have genuine saved[14,8] outcomes, TR0x30, DF entry ESP at stack_top-4,
liveNT1/HLT1 terminal and matching before/after images. Case1 image digest is
fd1dbf58742df124344112f28c9818651a120fba88ed22931c1568040e753e03;
case2 is388bb6f5e05d21c9f0ababaa8a7becfc42be228192ba660c1be7afad3fe77940.
The raw RTC stream reconstructs the stated nine-byte pre-window block; no
post-window RTC events are recorded. This remains trace reconstruction without
new model/calibration attestation.

The accepted62+4 assertion and66 compatibility results are retained source/
receipt facts, not results executed by this re-review. Revision1 preserves
their healthy serial and exact-ELF/inlined-halt rules and does not promote
them into evidence for A1/R1/P1 or general stack-overflow safety.

## This Review's Action Count And Limits

Builds0; tests0; host-control executions0; QEMU launches0; fault injections0;
new fault fixtures0; production mutations0; reviewer-issued Git write commands0.
Only new review documents/receipt-generator files and their outputs were written
in the isolated review directory. Receipt scripts hash/parse immutable files and
decode existing machine code; they do not import any kernel fixture, test,
observer, prior control runner or The END Engine module.

No process-history attestation proves universal absence of all possible prior
executions. Content equality also cannot exclude transient writes or same-byte
rewrite. The defensible result is the current bounded content/ref comparison
and explicit zero experimental actions taken by this review.
