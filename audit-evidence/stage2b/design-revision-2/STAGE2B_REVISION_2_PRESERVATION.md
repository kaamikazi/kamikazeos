# Revision 2 Preservation And Action Record

Date: 2026-10-29, Asia/Dhaka. This document records current read-only content/
identity inspection and the qualified historical facts. It grants no execution
authority and makes no universal process-history or tamper-resistance claim.

## Production

| Item | Current observed value |
|---|---|
| Repository | ${REPOSITORY} |
| HEAD | dbf23ce74a2a7103ae6fce04fba51969b7807178 |
| Tree | 62ff1f4fbd5d70163c074fb7c259f4f2115c4d77 |
| Branch | feature/v2.6.2-memory-protection-phase-b |
| Working tree | Clean |
| Production/evidence paths |6507 present and SHA-256 matching;0 missing,0 changed,0 additions in declared scope |
| Original recovery image |4daf871c5c2982a2c6401a8a3336959f31bdd0143ae00f238f4cbdc7f453f19e |
| Existing production ELF |7cc1262fb1caee2b104d17d601a19e172eb02eac6d93b56ad2b2612a228db7bc |

The scope uses the unchanged Revision1 inventories: tracked production files,
audit-evidence, build/test-runs, build/preservation, production ELF and original
recovery image. Metadata inspection read/hash-checked these existing files;
none was rebuilt, attached to a VM, formatted, restored, rewritten or deleted.
All source edits were limited to new design documents outside production.

## Historical Facts Retained Without Repair

The latest re-review found6507 production/evidence files matching and7279 of
7343 historical paths present/matching, with64 absent paths originally recorded
as empty. It also found matching branch/tag refs and two then-current
Codex-internal ref differences, with cause/timing unknown. These historical
findings are retained explicitly; Revision2 does not replace them with an
unqualified claim that everything was preserved.

The current content readback repeats the same counts:

| Historical directory | Expected | Present/matching | Absent |
|---|---:|---:|---:|
| kmz-v262-phase-a-review |1285|1275|10|
| kmz-v262-phase-b-review-dbf23ce |1368|1352|16|
| kmz-v262-phase-b-stage1-dbf23ce |2536|2498|38|
| kmz-v262-phase-b-stage2a-dbf23ce |463|463|0|
| kmz-v262-phase-b-stage2a-revalidate-dbf23ce |822|822|0|
| kmz-v262-stage2a-acceptance-dbf23ce |829|829|0|
| kmz-v262-stage2b-design-dbf23ce |21|21|0|
| kmz-v262-stage2b-adversarial-review-dbf23ce |19|19|0|
| Total |7343|7279|64|

Every absent path's recorded digest is the empty-file SHA-256
`e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`.
Surviving files have0 content mismatches and no new historical paths were found.
The complete path lists and digests are retained in provenance. Empty-file
absence is still an existence mismatch; it is not silently treated as an
unchanged file. No causal attribution to the revision author is supported.
No file has been recreated, restored or deleted to force inventory equality.

The original failed Stage2A tree, revalidation tree and accepted Stage2A tree
remain wholly present/matching within this inventory. Original failure remains
passed=false/exit1, digest
`70600df8e31af2bfc35235bc03310c913160fd58c5ef59cc39ef68abcca19a1a`.
Revision1 and the latest re-review artifact manifests also match every covered
file. No Stage2A claim is re-executed, reinterpreted as Stage2B evidence or
retrospectively repaired.

## Current Ref Qualification

Current branch/tag refs continue to match the latest review. The full internal
map has changed again: comparing this readback to the latest review shows
three path differences, all within `refs/codex/turn-diffs/`:

- The prior captures/1793248237470/.../base path is absent now.
- A captures/1793251775068/.../base path is present now.
- A checkpoints/d40e19b360b04e56170bc69dd4fa6b8c/.../1793249057192/... path is
  present now.

The two added refs resolve to the same production tree. The full exact names/
values and the separate delta versus Revision1 are in provenance. This newer
observation does not rewrite the latest review's earlier two-difference finding.
The internal names alone do not prove their creator or timing. No Git write,
cleanup, checkout, commit, reset, merge, rebase or push was issued. No ref was
restored/deleted to force equality. Read-only Git queries used optional locks
disabled; one unquoted tree query failed in PowerShell and was corrected by
quoting the revision, without any write operation.

## New Artifact Scope

All outputs are in the new isolated directory:

`${LOCAL_REVIEW_OUTPUTS}\stage2b-design-rev2-20261029`.

The ten requested Markdown/JSON deliverables and SHA256_MANIFEST.json are the
entire new artifact set. Proposed future symbols, manifests, calibration cases
and observer schemas are written contracts only. No executable oracle/control,
build script, binary, fault fixture, calibration input or disk image is created.
The provenance generator was a read/hash/document-metadata command, not a target
module import or a behavioral workload.

The manifest covers all ten payload files, including provenance. Its own digest
is supplied separately in the handoff; a manifest cannot contain its own SHA-256
without a circular definition. It explicitly excludes only itself. Existing
review/historical manifests are never regenerated.

## Action Counts And Status

| Action | Count |
|---|---:|
| Builds |0|
| Tests |0|
| Host-control executions |0|
| QEMU launches |0|
| Injections |0|
| Fault fixtures created/activated |0|
| Production mutations |0|
| Git write operations |0|

Only source/symbol/Git/file-hash inspection and new document/provenance writing
occurred. No previous build/test/fixture/observer/receipt-generator file was
executed, no guest CMOS operation occurred, and The END Engine was not modified
or imported. Content equality does not prove absence of every historical or
transient same-byte write; zero action counts describe this task's activity.

F02 remains partially resolved with U-BUILD/U-RAM open; F04 remains partially
resolved with CMOS-T1 unverified; F05 remains unresolved operationally. A1-E
stays blocked. H0-I consideration remains separate and unchanged. Revision2
is submitted for review, not independently accepted or authorized.
