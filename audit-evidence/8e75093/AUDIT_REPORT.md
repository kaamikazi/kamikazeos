# KamikazeOS v2.6.1 release audit at 8e75093

## Recommendation: REVISE

The latest corrections close the previously supplied eight failing adversarial
cases under the reproduced conditions. All 93 repository regressions and all
22 preserved audit checks pass on a fresh build. Additional independently
authored fixtures expose remaining snapshot replacement and package-authority
gaps. Acceptance is therefore not justified yet.

No repository source, exported product source, commit, branch, original image
or preserved evidence archive was modified. New fixtures and all outputs are
confined to this scratch directory. No CPL3 execution was enabled or performed.

## Provenance and independent build

- Repository: `E:\the legendary os`.
- Branch: `hardening/v2.6.1-integrity-gate`.
- HEAD verified before/after: `8e75093f1d880d8b546d4f0c8f0d6e7ef9506086`.
- Git tree: `2cbb1f13152ed2dba4ad453495e8c5f39cecf7e3`.
- Working tree clean before/after; no commits, merges or pushes.
- Read `docs/RELEASE_BLOCKERS.md`, `docs/INTEGRITY_FOLLOWUP.md`, actual source,
  and the preserved ZIP, rather than accepting their test claims as results.
- ZIP SHA-256 matches `bae909075761af2c3edafe33e993002487d79cc61c9c0dae483ffccb2b75be75`;
  all ZIP CRCs pass. It contains 1,166 files / 134,947,572 uncompressed bytes.

`prepare.py` exports each pinned commit using Git, verifies canonical Git blob
hashes with the repository's clean filter (Windows line endings), independently
enumerates C/assembly translation units, compiles with Clang and links with LLD.
It does not invoke the implementation's build script or reuse its ELF files.
Every exported source blob was reverified after all tests.

| Build | Commit | Verified source blobs | Units | ELF SHA-256 |
|---|---|---:|---:|---|
| Target | `8e75093` | 262 | 45 | `7e295a57c7bea3a2d9130f1bce79ea1a16ad19a77dd3beaefe6fb048fe320a6a` |
| Baseline | `f6c1814` | 225 | 44 | `5c1314ec2cbadf9b3751e5e92510b1dfe851cecf095dc14a39a47090f606a726` |
| Previous audit target | `0cd86d8` | 250 | 45 | `8b961369c26c5fa8999eff56bfa9a3c438cf6cffff1c0240cdf1ed527351591b` |

Tools independently queried: Clang/LLD 22.1.7, QEMU 10.2.0, Python 3.10.11,
Node.js 24.16.0. Different scratch paths affect debug information; matching the
implementation ELF hash is not substituted for source provenance.

## Exact observed test totals

| Suite | Cases | Passed | Failed cases | Errors | Skips |
|---|---:|---:|---:|---:|---:|
| Complete supplied repository suite | 93 | 93 | 0 | 0 | 0 |
| Preserved adversarial QEMU checks | 17 | 17 | 0 | 0 | 0 |
| Preserved actual-C host checks | 5 | 5 | 0 | 0 | 0 |
| **New independent QEMU fixtures** | **17** | **6** | **11** | **0** | **0** |
| **New independent actual-C host checks** | **6** | **3** | **3** | **0** | **0** |
| All executed cases across these suites | **138** | **124** | **14** | **0** | **0** |

The 93-case suite took **812.135 s**. The preserved 17-case QEMU suite took
**147.531 s**. New QEMU cases ran in two disjoint runs: 16 cases in **133.812 s**,
then one additional staged-operation case in **12.019 s** (total **145.831 s**).
They contain **25 failing assertions/subtests across 11 failed cases**; those
are not 25 separate failed cases. Host assertions execute actual committed C compiled
to WebAssembly with mocked I/O, not reimplemented kernel algorithms. They do
not prove hardware privilege transitions or ATA atomicity.

The preserved host checks required only redirected source include paths and a
64-sector capacity mock; their assertions were unchanged. The preserved QEMU
script was unmodified. No fixture errors or skipped cases occurred in these runs.

## Audit A: FAIL -- journal semantics and snapshot replacement

### A1. Scalar domain consistency: PASS

The shared validator consistently checks magic/version, event numeric bounds,
result/reversible booleans, risk enum, non-package ID bounds, zero reserved byte
and printable zero-padded tags. Scan/getter/lifecycle lookup all use it. The
previous boot-240 scalar-invalid fixtures pass, and occupied bytes remain
preserved. Unsupported numeric event codes are rejected. Printable tags are
documented as opaque labels, not a closed code catalog; unfamiliar printable
labels alone were not treated as an exploit in this audit.

### A2. Event-specific semantics/policy: FAIL

`recovery_store.c:151-170` checks independent fields, not their meaning for each
event. The fixed producers define successful CLEAN_BOOT as action 0 / result 1 /
risk 0 / reversible 1 (`kernel.c:287-288`), and emit successful SUGGESTED events.

New fixtures, with every scalar field in range and a correct checksum, show:

- `SUGGESTED, action=1, result=0` is exported and treated as an already-issued
  suggestion, suppressing the legitimate suggestion after action rollback.
- `CLEAN_BOOT, action=1` is accepted as ordering evidence.
- A successful/reversible low-risk PANIC tuple is accepted as ordering evidence.

These boot-240 records raise the reported next boot to 241. The new gap guard
does preserve existing snapshots in those three isolated high-bound cases;
that is an improvement over the previous target. The remaining failure is
event-specific consistency and, particularly, treating a refused suggestion
as a successful deduplication fact (`recovery_store.c:657-661`).

The supplied `journal_domains_fixture.c` intentionally tests all combinations
of in-range result/risk/reversible values as valid for all event types. That
does not establish requested event-specific semantic validation. A precise
supported tuple contract should be defined; at minimum failed/refused events
must not supply the same policy proof as successful events. These observations
are not claims of checksum authentication or rejection of arbitrary opaque tags.

### A3. Progression proof accepts contradictory chronology: FAIL

`recovery_journal_boot_progression_verified()` (`recovery_store.c:676-704`)
builds a bitmap of boot numbers. It does not verify that qualifying CLEAN_BOOT
records form a chronological chain in the append-only journal.

Reproduction `Progression.test_reverse_clean_boot_sequence_is_not_a_chain`:

1. Seed a complete valid snapshot ring for boots 1-16, valid state at boot 16,
   and journal records CLEAN_BOOT 32, 31, ... 1 in that physical order.
2. Boot the target with this disposable image.
3. The journal's numeric lower bound selects boot 33.
4. The unordered membership bitmap accepts boots 17-32 as corroboration.
5. LBA 17 changes from snapshot boot **1 to 33**, despite contradictory order.

An ordered 1..32 control passes legitimate rotation. A missing number, a bad
checksum in a required gap record, and later self-generated clean records
without intervening history all correctly hold snapshot writes. Thus the tested
missing-history path does not circularly repair itself; the remaining gap is
accepting an unordered set as a sufficient chronological proof.

Separately, a checksum-valid record labelled CLEAN_BOOT 17 with risk=high and
reversible=0 completes a gap from snapshot 16 to current boot 18. It replaces
LBA 18 snapshot **2 with 18**. Only action/result are checked by the corroboration
helper, so its trust inherits the event-semantic weakness above.

These fixtures do not require detecting malicious rewriting of all mutually
consistent metadata: journal order or event meaning is itself inconsistent.

### A4. Ring destination can contain the newest snapshot: FAIL

This is independent of journal semantic interpretation and fake high counters.

Reproduction `Progression.test_misplaced_latest_snapshot_is_not_oldest_ring_slot`:

1. Seed valid state/journal and all 16 snapshots for boots 1-16.
2. Swap only LBA 16 (boot 16) and LBA 17 (boot 1). Each sector still has a valid
   checksum, but ring placement is inconsistent with `boot % 16`.
3. Boot normally as boot 17 with fully consecutive progression.
4. The preflight accepts both sectors because it checks record validity and
   `record.boot < current.boot`, not placement/destination age.
5. LBA 17 changes from **boot 16 to boot 17**: the newest historical snapshot is
   destroyed instead of the oldest one.

Source: `boot_snapshot.c:152-175` and `:208-210`. Validate ring consistency and/or
the actual destination record's permissible age before replacing it. A global
latest-boot check alone is insufficient. Before/after complete snapshot bytes
are retained in this case's directory.

### A5. Other requested controls

- Normal 18-boot rotation: PASS in the independently executed supplied suite.
- Half-range state conflicts, generation rollover/max values, boot exhaustion
  and no boot-zero reuse: PASS in supplied and preserved checks.
- Missing/corrupt gap records: PASS in new controls.
- Non-circular behavior for later CLEAN_BOOT records after an unsupported high
  bound: PASS in the new three-restart control.
- Occupied journal-prefix preservation: PASS in new cases where compared.
- Blanket historical snapshot preservation: FAIL in the cases above.

## Audit B: FAIL -- package-copy quorum is not usable slot truth

### Previous package-copy blockers: PASS

Primary/mirror read/checksum failures, missing metadata, damaged sidecars,
unbound active legacy data and package generation ambiguity hold mutations as
reported. Healthy-state ABA rejection, legitimate-owner rollback, staged
abort/recovery, single-copy acknowledged writes and tagged downgrade/re-upgrade
pass their supplied/preserved tests.

A new actual host-import plus `seed-manifests confirm` fixture confirms that
importing a valid manifest does not clear an uncertain package-copy hold. Apply,
approve and reset still leave package/slot sectors unchanged. A 56-sector image
is correctly `unavailable` because LBA 56 is absent. Host no-disk/fresh-capacity
transition controls also pass.

### New package trust boundary failure: FAIL

`package_state.c:294-333` grants `verified` / `writes_allowed=yes` based on the
two package-state sectors. Except for the all-zero fresh-store branch, it does
not establish that module-slot evidence is readable, valid or consistent with
the current package owner.

`module_slots.c:87-137` starts with factory defaults and silently retains them
when its sector read/checksum/header fails. Those defaults are pushed into the
running version table. `pkg_apply()` trusts that RAM version for from_version
and pending-verification checks (`recovery_package.c:408-421`).

Four independent real-QEMU cases:

1. Complete and verify PKG-201 -> PKG-202 -> PKG-203. Preserve both valid current
   package copies, which identify the current 0.4 transition and retire old rights.
2. Independently test: LBA 56 read-only I/O error; checksum corruption; erased
   slot sector; or a valid older slot sector saved after PKG-202 (active 0.3).
3. Boot. All four cases report `ownership_state=verified`, `writes_allowed=yes`.
4. Run `approve-pkg PKG-201`, then `apply-pkg PKG-201`.
5. All four produce `PKG-ACT-002` and modify package/slot sectors. Factory or
   stale 0.3 metadata incorrectly authorizes a 0.3->0.4 apply even though the
   current committed package owner was PKG-203 at 0.4.

For the read/checksum/erased cases, the pre-command report presents synthetic
factory A=0.3/B='-' slots while still presenting PKG-203's valid committed 0.4
package state. After the accepted apply, PKG-201 owns an unverified B=0.4,
pending_verify=1 transition and PKG-203's authority is retired. The stale valid
slot case likewise contradicts the current package state yet permits mutation.

The read fault was explicitly restricted to reads; guest writes remained
possible. These are simulated module-metadata mutations, not claims of actual
binary module execution. The four cases preserve prior journal prefixes but
overwrite the questionable package/slot metadata.

Required direction: propagate slot-read/validation uncertainty to the package
authority gate, and reconcile usable slot truth with current/staged ownership
before authorizing transitions. Unknown defaults must not become verified
from_version or pending-verify evidence. This audit does not implement a fix.

### Staged-operation evidence is also rewritten automatically: FAIL

`SlotTrust.test_unreadable_slot_cannot_abort_durable_staged_apply` isolates a
post-slot/pre-package-commit interruption, using real PKG-201/202 updates and a
real PKG-203 approval as the starting records. Both package copies are valid
and identical, with PKG-203 APPLY_STAGED A->B / 0.3->0.4 intent; LBA 56 contains
the completed B=0.4 switch, rollback=A, pending_verify=1. No PKG-203 apply or
verify event has been journaled, matching the interrupted transaction boundary.

On a target boot with only LBA 56 reads failing:

- Module-slot RAM defaults claim A=0.3.
- Package state is classified `verified`.
- `recover_interrupted_transactions()` interprets those defaults as a staged
  apply that never switched (`recovery_package.c:238-242`).
- It logs `PST-RECOVER-ABO` / "Discarded staged apply before slot commit" and
  rewrites both package sectors from APPLY_STAGED to COMMITTED/APPROVED.
- The actual B=0.4/pending slot sector stays unchanged, but staged intent is lost.
- After clearing the read fault and rebooting, PKG-203 remains **approved**, not
  **applied**; the original staged evidence is no longer present to reconcile.

This reproducer invokes no package mutation command during the faulted boot.
It therefore demonstrates an automatic evidence rewrite and persistent recovery
misclassification, beyond the command-driven version-gate bypass above. The
fixture models an interrupted sector-level transaction; it is not a physical
power-loss atomicity test.

### Trust-state assessment

| State | Observed assessment |
|---|---|
| `fresh` | PASS for tested readable-empty state/slot predicate and capacity controls; this is not authenticated anti-rollback proof |
| `verified` | FAIL as sufficient mutation authority: current package copies can coexist with unusable or contradictory slots |
| `degraded_current` | PASS for the supplied acknowledged-write/restart model; physical write atomicity remains unverified |
| `uncertain` | PASS for exercised package-copy failures, import/seed/reset and preserved-sector holds |
| `unavailable` | PASS for exercised no-disk/insufficient-capacity holds |

## Audit C: FAIL overall; regression execution/safeguards PASS

The independently built target passes all reported 115 supplied/preserved
checks, including TinyFS, package policy/canaries, reports/analyzer/dashboard,
small disks, snapshots under explicit locks and privilege self-tests. These
passing checks do not imply the new A/B integrity fixtures passed.
C's blanket snapshot/metadata preservation requirement fails in those new
cases. TinyFS, ordinary package policy, compatibility, execution holds and the
original-image/worktree preservation checks pass as individually reported.

Storage compatibility: **PASS for encoding/readability and tested round trips**.
No incompatible migration is observed. The package-sector encoder is identical
to `0cd86d8`. New ownership fields are RAM/report fields. Existing sector owners:
state 1/50; journal 2-15; snapshots 16-31; manifests 32-39; package state 48/49;
slot record 56; TinyFS 64-127. Existing recovery extension/mirror compatibility
with baseline f6c1814 is exercised by downgrade/re-upgrade tests.

Legacy/unbound package data intentionally stays held; compatibility does not
mean old-kernel mutations inherit new authorization policy. The reported
small-disk modes (32/50 single-copy, 51/2048 mirrored RecoveryStore) pass.

Ring 3 source approval remains zero in all independently built inputs. Held
execution tests pass, and every new QEMU report asserts both execution fields
are disabled. No live CPL3 transition occurred.

Original `build/recovery.img` SHA-256 before and after the audit:

```text
4daf871c5c2982a2c6401a8a3336959f31bdd0143ae00f238f4cbdc7f453f19e
```

The original preserved ZIP is unchanged. Original source HEAD/worktree and all
scratch exported source blobs were independently reverified after testing.

## Remaining unproven properties and acceptance work

- REVISE for unusable slot evidence authorizing package mutations, destination
  snapshot-age/geometry validation, and stronger chronological/event-specific
  corroboration. The prior scalar/copy-fallback fixes themselves pass replay.
- Physical torn-sector, power-loss and write/flush atomicity remain UNVERIFIED.
  Return-code fault injection and mocked host saves do not establish them.
- Checksums/CRC are not authentication; coherent rollback/forgery of all relevant
  metadata is not prevented. Kernel signature verification remains simulated.
- Generations retain the serial half-range assumption; boot counters remain
  8-bit with the observed exhaustion hold. Conservative availability holds
  remain intentional, and old kernels retain historical policy defects.
- The current paging baseline still has known memory-protection limitations;
  they are outside these storage findings. No v2.6.2 implementation was begun.

Because the recommendation is not ACCEPT, the conditional v2.6.2 milestone
proposal is deferred. Address and independently re-audit the new integrity
counterexamples before accepting this release.

## Evidence and reproduction

- `provenance.json`, `*-build.txt`, `target.zip`, `baseline.zip`, `previous.zip`.
- `supplied-93/summary.txt`: 93 supplied regressions.
- `preserved-qemu/summary.txt`, `preserved-host-results.json`: 22 replayed checks.
- `new-qemu/summary.txt`, `new-qemu/results.json`, per-case `observed.json`, raw
  input sectors, snapshot before/after bytes and serial logs: new QEMU results.
- `new-qemu-stage/summary.txt` and its per-case inputs/observations: the additional
  automatic staged-abort reproduction.
- `new-host-results.json`, `new_store.c`, `new_package.c`: new actual-C checks.
- `audit-evidence.json`: final totals, observations and protected-input hashes.

From this scratch directory, using a new run folder:

```powershell
python -B new_cases.py --run-dir <new-scratch-dir> -k misplaced_latest
python -B new_cases.py --run-dir <new-scratch-dir> -k reverse_clean
python -B new_cases.py --run-dir <new-scratch-dir> -k slot_read_error
python -B new_cases.py --run-dir <new-scratch-dir> -k unreadable_slot_cannot_abort
python -B new_cases.py --run-dir <new-scratch-dir>
python -B run_new_host.py
```
