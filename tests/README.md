# KamikazeOS regression tests

Headless QEMU tests for the v2.6.1 "Integrity Gate" hardening. Every test
boots the real kernel with `-display none`, drives the shell through the
QEMU monitor's `sendkey`, and reads evidence back from the serial log.

```
python tests/run_regression.py                  # build + run everything
python tests/run_regression.py --no-build -k F1 # one group
python tests/run_regression.py --baseline-elf old.elf   # + downgrade test
make test                                       # same, via make
.\scripts\run-tests.ps1                         # same, on Windows
```

## Safety rules the harness enforces

- Each test creates its own **disposable** 1 MiB disk image under
  `build/test-runs/<timestamp>/<test>/` and keeps the serial log,
  analyzer output and dashboard JSON next to it for review.
- `build/recovery.img` is never attached to QEMU or written. The runner hashes
  it before and after the run and exits with code 3 if it changed.
- Failure injection (torn appends, bad checksums, both state copies
  damaged, equal-generation conflicting copies, full journals, corrupted
  TinyFS data) only edits those disposable images, between power-ons.
- No test approves or runs CPL3 code. The suite asserts that Ring 3 stays
  held (`execution_approved=no`) and that the in-kernel privilege
  self-tests (interrupt-frame layout, syscall origin policy, register
  preservation) still pass at every boot.
- Primary/mirror/journal read errors use QEMU `blkdebug` under the raw
  image node on primary IDE unit 0. These rules alter I/O outcomes without
  editing image bytes. Tests assert the lock and unchanged snapshot sectors.

## Groups

| Group | What it proves |
|---|---|
| F1 journal integrity | torn appends, damaged state, corrupt entries, full and ambiguous journals never lose or overwrite evidence; legacy images upgrade in place; v2.6.0 boots v2.6.1 images |
| F2 package rollback | two sequential updates to one module; stale rollbacks and applies over unverified updates are refused |
| F3 manifest seeding | seeded built-ins stay usable, never overwrite imported manifests, and are not labelled as signed |
| F4 health score | typos, canaries and intentional tests do not cost health; real faults do; ring wraparound cannot raise the score |
| F5 journal longevity | unchanged suggestions are not re-journaled; a full journal costs health once |
| R1 regression | Ring 3 hold, TinyFS, recovery actions, canaries, host manifest tooling, analyzer and dashboard compatibility |
| D1 small disks | capacity-aware single-copy mode at 32/50 sectors and mirrored mode at 51/2048; state and journal survive restarts |
| D2 snapshot evidence | locked/conflicting/unreadable state, unreadable journal, reconstruction, future snapshots and exhausted boot counters |
| L1 generations | legacy-primary flags win without resetting a validated higher mirror generation |
| L2 health policy | actual registry/log/health C compiled to WebAssembly; verified fallback after a block is still a production warning |
| L3 intentional scope | returning-trigger wrapper tested on the host; actual divide/invalid-opcode/page faults tested at CPL0 in QEMU |
| N1 legacy recognition | damaged extension magic and nonzero reserved tails cannot masquerade as a legacy primary |
| N2 generation ordering | half-range ambiguity in both stores, conflicting package copies, normal ordering and generation wrap |
| N3 journal readers | unsupported versions/events are preserved but neither exported nor used to suppress suggestions |
| N4 rollback ownership | ABA reuse, durable supersession, old ambiguous claims, staged apply abort/switch, tagged downgrade/re-upgrade, and actual C commit/reconciliation save failures |
| R5 release journal | malformed field domains at boot 240, evidence/export/suggestion isolation, snapshot-gap corroboration, healthy 18-boot ring rotation, and actual C domain/boundary checks |
| R6 release ownership | stale primary/mirror read/checksum failures, missing or legacy copies, sidecar corruption, erased state with slot history, actual interrupted writes, degraded current ownership and fault-clear reboots |
| T1 snapshot authority | swapped/duplicate physical sectors, exact victim age and boot boundaries, healthy destination and unchanged refused history; actual-C preflight plus QEMU |
| T2 event contracts | actual producer tuples, successful-only suggestion facts, rejection before high boot bounds and healthy rotation without poisoned progression |
| T3 append chronology | physical order, duplicate/missing completions, sector boundaries, no boot wrap, revalidation and failed-clear lock retention |
| T4 joint authority | actual slot failure/contradiction, preserved stage intent, cold recovery, reload latch, durable transition edges and scoped/partial reset; real persistence/slot/engine C plus QEMU |
| T5 slot transitions | per-module durable intent, forward-only changes, mixed-proposal rejection, unaffected fields, staged retries/reconciliation, and two-module QEMU lifecycle/reset |
| T6 reset boundary | exact factory slot image (identity, order, uniqueness, version-array tail bytes, flags) and one-shot reset grant (revoked on rejection, out-of-order and I/O failure; consumed on success); actual authority/slot/state C |
| M1 Phase A layout/policy | production pure-C permission model, all 4096 active leaves and five user pages, invalid spans, ELF boundaries/subsections, inactive guards and ordinary QEMU boot |
| M2 Phase A RAM accounting | reserved-wins 64-bit RAM union, tracked/mapped/reserved/candidate eligibility, untrusted discovery, high/partial pages and RAM limits; actual pure C |

The completed v2.6.1 run contains **148 cases**, including **13 T6 reset-boundary
cases**, with compatibility tests enabled by `--baseline-elf`. The verified full
run, source/fixture hashes, before/after reproductions and remaining limits are
recorded in [RESET_BOUNDARY_CORRECTION.md](../docs/RESET_BOUNDARY_CORRECTION.md).

Phase A preserves those 148 IDs. Its verification deliberately excludes the
three `L3Ring0Faults` cases and the intentional panic test; no CPU fault injection
or CPL3 transition is approved in this phase. `--exclude` records exact omitted
IDs separately from results, rather than counting them as passed or skipped.
See [Phase A evidence and scope](../docs/MEMORY_PROTECTION_PHASE_A.md).

The host fixtures require LLVM's wasm32 backend and wasm-ld plus Node.js.
They mock port/state APIs and never emulate CPU privilege transitions. The
QEMU tests cover actual exception entry and persisted evidence separately.
Ownership host fixtures execute actual package transaction C with deterministic
state-save failures. They verify multi-record rollback on failed persistence;
they do not simulate physical ATA torn sectors or power interruption.

## Test fixtures and trust

Chained same-module package tests need signed manifests that no real
example provides. `kmz_disk.fixture_manifest_sector()` writes the
host-verified marker byte directly into the disposable image. That marker
is exactly what the kernel's **simulated** signature check reads, so the
fixture stands in for "a manifest the host tooling verified". It is not a
signature and is never written to `build/recovery.img`.
