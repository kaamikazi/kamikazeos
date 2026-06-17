# Safety Principles

KamikazeOS treats recovery as a controlled process, not a magic automatic fix.

## Rules

- No networking.
- No driver downloads.
- No arbitrary package loading.
- No binary patching.
- No kernel self-modification.
- No AI auto-apply.
- Every recovery action comes from a fixed catalog.
- Every package must pass deterministic policy checks.
- Human approval is required for changes.
- Analyzer and dashboard are read-only.

## Package Blocking Rules

The kernel blocks packages that are:

- unknown
- unsigned
- invalid
- tampered
- checksum-failing
- trust-anchor-failing
- high risk
- irreversible
- blocked payload type
- unknown target module

Host tools help catch problems early, but the kernel is the source of truth.

## Dashboard Contract

Recovery Center may show recommendations and copy shell commands. It cannot
apply changes, write the recovery image, call the kernel, or download updates.
