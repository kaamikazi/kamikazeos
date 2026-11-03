# Stage 2B Design And Review Archive

Published: 2026-11-03 (Asia/Dhaka). This is a documentation backup, not a release,
acceptance decision or authorization to run experiments.

## Reading Order

1. [Original design](original-design/STAGE2B_DESIGN.md).
2. [Original adversarial review](original-review/STAGE2B_INDEPENDENT_REVIEW.md).
3. [Design Revision 1](design-revision-1/STAGE2B_DESIGN_REVISION_1.md).
4. [Revision 1 re-review](revision-1-review/STAGE2B_REVISION_1_INDEPENDENT_REVIEW.md).
5. [Design Revision 2](design-revision-2/STAGE2B_DESIGN_REVISION_2.md).

Revision 2 is the latest archived design, **not** a verified implementation.
F02 remains partially resolved (actual build/RAM evidence missing), F04 remains
partially resolved (CMOS observation/calibration unverified), and F05 remains
unresolved (complete operational writer coverage not demonstrated). A1 execution
is blocked. H0-I is ready only for owner consideration of host-data implementation,
not for execution. All later authority gates remain separate.

## Publication And Evidence Boundaries

These are sanitized publication copies of the existing Markdown reports and
selected JSON provenance, reference receipts and historical manifests. Local
absolute paths are replaced with `${REPOSITORY}`, `${LOCAL_EVIDENCE}`,
`${LOCAL_REVIEW_OUTPUTS}`, `${USER_HOME}` or `${SYSTEM_DRIVE}`. JSON formatting and
text line endings are normalized. No technical finding or authorization verdict
is intentionally changed. Originals remain untouched outside this repository.

`PUBLICATION_MANIFEST.json` lists each included document's original SHA-256 and
published SHA-256, its source collection label, and the exact transformation
policy. Its published hashes, not historical manifests, identify these copies.
Historical manifests and the hashes quoted inside reports refer to original
inputs; they must not be interpreted as checksums of sanitized publication bytes.
Manifest self-hashes are excluded to avoid circularity.

This is a curated report archive, **not a complete raw evidence backup**. Large
file inventories, raw disassembly, ELF binaries, disk images, trace logs, source
ZIPs and inspection scripts remain local and are not uploaded. References to
those files describe historical inputs, not files available in a fresh clone.
The manifest lists omitted source-directory filenames without their contents.
Published provenance may reference additional historical directories that were
inputs to those reviews; those directories are not included here.

The reports' statements about clean trees, Git-write counts, production hashes,
observations and dates describe their original review sessions at production
baseline `dbf23ce74a2a7103ae6fce04fba51969b7807178`, not this later archival commit.
Their preservation qualification (64 absent historical paths recorded as empty)
is retained. Publishing the documents does not resolve that qualification.

The Revision 1 re-review discloses that it was an AI assessment in an existing
conversation, not an external human audit or a fresh-context independent-agent
approval. Preserve that distinction when citing these materials.

## No Runtime Change

This archival operation does not build or boot the kernel, run host controls,
calibrate observers, inject faults, enable protection or Ring 3, alter policy,
change the disk layout or touch `recovery.img`. Experimental feature branches
remain separate from `main`. Public availability is not production readiness.
