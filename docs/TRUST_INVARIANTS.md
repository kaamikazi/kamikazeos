# v2.6.1 persistent-mutation trust invariants

This design was written **before kernel behavior changes**, after reproducing
the independent REVISE review of `8e75093`. It is a required consistency model,
not independent release acceptance or a claim of authenticated/crash-atomic
storage. Implementation evidence will identify which checks enforce it.

## Scope and evidence

- Ring 3 approval remains zero. No original `build/recovery.img` is attached
  to tests or written. Tests use disposable images.
- Checksums detect some damage, not hostile rewriting or coherent rollback.
  The fixed package model changes version/slot metadata, not executable code.
- A readable sector is not necessarily valid; a valid record is not necessarily
  current; a quorum of package copies is not joint ownership authority.
- Factory defaults are diagnostic unless positively established as a pristine
  baseline. Failed/missing/corrupt slot reads never establish defaults as truth.
- Existing disk fields are retained. Ambiguous evidence is held rather than
  repaired, guessed or upgraded. A persistent-identity/format change requires
  separate migration approval before implementation.

## All persistent owners and permission boundaries

| Owner / range | Required permission |
|---|---|
| ATA driver | Non-null bounded sector request within IDENTIFY capacity; successful operation/flush return is the model's durability acknowledgement, not a torn-sector proof |
| Recovery state, LBA 1/50 | Valid capacity mode, unambiguous generation selection, complete readable journal scan, trusted boot ordering and no exhaustion/uncertainty lock; mirror-first saves never erase journal/snapshot evidence |
| Journal, LBA 2-15 | Shared event-semantic validation; proven append position after all occupied slots; selected slot verified zero immediately before writing; no wrap/reuse when full; unreadable or contradictory ordering cannot authorize destructive history writes |
| Snapshots, LBA 16-31 | Full valid history, correct physical boot position, unique/non-future records, trusted chronological progression and an empty or sufficiently aged intended destination |
| Manifest sectors, LBA 32-39 | Fixed schema/policy validation and bounded sector ownership; kernel seeding is explicitly confirmed and fills only empty sectors; offline host import changes manifests only and grants no package authority |
| Package state, LBA 48/49 | Joint package/slot authority, a coherent proposed state transition, valid ordered package redundancy and acknowledged staging before slot changes; no persistence from an uncertain fallback |
| Module slots, LBA 56 | Joint authority, a durable matching staged package intent and a coherent proposed slot transition; explicit pristine initialization/reset are narrowly scoped exceptions below |
| TinyFS, LBA 64-127 | Existing fixed-range, bounded metadata/data validation and explicit filesystem/app operations; formatting requires confirmation; no package/recovery authority is acquired from file contents |
| CMOS recovery fallback | Bounded diagnostic/policy recovery flags only; CMOS never unlocks uncertain disk ordering or package ownership |

Confirmed destructive recovery clear is limited to its documented state/journal
and CMOS ranges. It cannot clear joint package uncertainty. Confirmed package
reset requires joint trust and cannot be used to bypass uncertain evidence.

## A. Joint package authority

### A1. Inputs and states

Authority is derived at one shared boundary from:

1. Read/validation/capacity status of both package sectors and their serial
   generation order (equal copies must be identical; half-range is unordered).
2. Read/validation status of the actual module-slot sector, including module
   IDs, bounded version strings, flags, active/rollback indices and pending state.
3. Bound package records, current manifest identities and their exact from/to
   versions, slot transitions, status flags and transaction phases.
4. Last acknowledged durable RAM snapshots of those records/slots, distinct
   from proposed mutable RAM contents.

Joint states:

| State | Permission |
|---|---|
| `unavailable` | Required disk/ranges or initialization are absent; no package/slot writes |
| `uncertain` | A read/validation/order/binding/ownership consistency check failed; no package/slot writes, reset, legacy binding or reconciliation |
| `fresh` | Both package sectors and slot evidence positively describe an unused/pristine factory baseline; only explicit policy-approved initialization/approval/apply can advance it |
| `verified` | All required readable evidence agrees on one usable current state or one uniquely recoverable staged transition |
| `degraded_current` | This session knows a requested valid write reached at least one current package copy; current causal ownership may continue, but cold boot revalidates all evidence |

Uncertainty is sticky for the current boot. Registry/manifest reload is not a
permission reset. A new boot can regain trust only by fully reading and jointly
validating the available records and real slots. Two stale package copies alone
cannot clear a slot contradiction. No reload may write to manufacture agreement.

### A2. Current committed state

- An eligible APPLIED/VERIFIED/FAILED record must be bound, committed, have
  distinct valid before/after slots and not be retired with `SLOT_NONE`.
- At most one current eligible claim per module is permitted. Retired records
  retain history but confer no mutation rights. Competing claims hold authority.
- Package identity equality is case-insensitive, matching registry/state lookup;
  aliases cannot select different records in validation and mutation code.
- APPLIED/FAILED ownership requires active=after/to, rollback=before/from,
  verified known-good rollback storage, pending verification and an unverified
  active slot. VERIFIED requires the same version/slot correspondence with the
  active slot verified and pending cleared.
- A completed rollback requires active=before/from, verified active storage,
  no rollback pointer and no pending verification. A new approval may retain
  that historical no-rollback baseline without acquiring old rollback rights.
- A pristine baseline must match the compiled factory arrangement exactly;
  missing slots beside active or historical ownership are not factory evidence.
- Approval cannot destroy a current ownership claim. All proposed metadata
  saves must remain consistent with the durable joint state, not only callers.

### A3. Staged transitions

Each staged record has one supported phase and a bound intent. Actual readable
slot evidence must match exactly one supported side of that intent:

| Phase | Before side | After side / permitted state publication |
|---|---|---|
| APPLY_STAGED | Real validated active before/from baseline; previous owner or pristine/rollback baseline remains coherent | Active after/to, pending/unverified, rollback before/from verified; publish APPLIED and retire older rights together |
| VERIFY_STAGED | Current applied transition, pending/unverified | Same active after/to, verified/not pending; publish VERIFIED |
| ROLLBACK_STAGED | Current applied/verified/failed transition at after/to | Active before/from verified, no rollback pointer, not pending; publish ROLLED_BACK |

A valid before-side apply can publish an abort to its saved approval state.
Unknown slots are **neither side**. They cannot justify an abort, failure mark,
commit, retirement or legacy binding. Inconsistent/competing intents hold writes.

Normal slot writes require already durable matching staging. A slot write that
returns failure makes slot evidence uncertain; restore RAM only for diagnosis,
retain the persisted staged intent and do not publish an assumed abort. A failed
state publication preserves staged intent. Cold recovery acts only after actual
slot evidence becomes readable and jointly coherent.

### A4. Initialization and explicit reset

Before a first staged apply, explicitly requested package use initializes the
proven pristine slot baseline in the existing slot format. This gives cold
recovery actual before-side evidence rather than assumed defaults. An empty
sector beside staged/active ownership is held, not automatically initialized.

Reset is a narrowly scoped RAM-only permission acquired from a trusted,
non-staged joint state by `reset-packages confirm`. It permits only empty package
state followed by the exact factory slot image, and is revoked on any failure.
It is not resumed automatically after a crash. A partial reset that leaves
inconsistent evidence holds on reboot; no new persistent reset phase is invented.

## B. Snapshot replacement

Before any snapshot write:

1. State/journal ordering is trusted, reads complete and boot count not wrapped
   or exhausted. Every occupied snapshot has supported magic/version/checksum.
2. Each occupied record is at `LBA 16 + (boot_number % 16)`. Duplicate boot
   numbers, misplaced/future/same-boot records and contradictory histories hold.
3. Consecutive progression or the journal proof below establishes the current
   boot's age relative to history. A numeric maximum alone is not proof.
4. The intended destination is zero or contains the record actually replaceable
   at that physical position: its boot precedes the current boot by at least 16,
   without unsigned underflow/wrap. A recent/newest record is not evictable merely
   because it has a valid checksum.
5. Failure at any preflight step leaves every historical sector unchanged.

Healthy consecutive boots retain the 16-slot ring, including boots 17/18.
Corroborated gaps can age a destination; unexplained gaps cannot.

## C. Chronological journal proof

- The journal is physically append-only. Sector boundaries do not restart
  chronology; a full journal does not wrap to slot zero.
- Valid event boot numbers cannot move backward. Multiple ordinary events in a
  boot are normal; more than one successful CLEAN_BOOT for the same boot is
  contradictory completion evidence. Invalid/nonzero entries stay occupied.
- When a gap must be justified, read the append stream in physical order. There
  must be exactly one canonical successful CLEAN_BOOT for each required boot,
  in increasing consecutive order. Missing, repeated, reversed or contradictory
  evidence does not authorize replacement. Sorting or membership sets are not
  a proof. Later self-generated clean records cannot fill an older missing gap.
- Boot numbers are 8-bit and do not gain serial wrap semantics; exhaustion
  holds persistence. Generation counters remain separately wrap-aware uint32.
- Legacy records remain readable forensic evidence even when chronology cannot
  grant mutation permission. No checksum is treated as authentication.

## D. Event-semantic contract (reviewed producers)

Sources: `recovery_actions.c` (fixed action catalog and journal_action),
`kernel.c` (boot events), `panic.c`, `recovery_package.c` (journal_pkg), plus the
same producer APIs in baseline `f6c1814` and audited `8e75093`.

All events require v1 magic/version, checksum, zero reserved byte and printable
six-byte zero-padded tag. Tags are opaque labels, not permission tokens or a
new closed catalog. Package IDs retain their documented complete byte range.
Action IDs refer to the fixed catalog: 1/2/3 are low-risk/reversible; 4 is
medium-risk/reversible; 255 is the high-risk/irreversible blocked canary.

| Event | ID | Result | Risk / reversible |
|---|---|---|---|
| SUGGESTED | 1-4 | 1 | catalog risk / 1 |
| APPROVED | 1-4 | 1 | catalog risk / 1 |
| APPLIED | 1-4 | 0 or 1 (producer has a failure path) | catalog risk / 1 |
| ROLLED_BACK | 1-4 | 1 | catalog risk / 1 |
| BLOCKED | 255 | 0 | high / 0 |
| PANIC | 0 | 0 | high / 0 |
| SAFE_MODE_BOOT | 4 | 1 | medium / 1 |
| CLEAN_BOOT | 0 | 1 | low / 1 |
| PKG_SUGGESTED (reserved supported lifecycle event) | package byte | 1 | low/medium / 1 |
| PKG_APPROVED, PKG_APPLIED, PKG_VERIFIED | package byte | 1 | low/medium / 1 |
| PKG_FAILED | package byte | 0 | low/medium / 1 |
| PKG_ROLLED_BACK | package byte | 0 or 1 (stale refusal is logged as 0) | low/medium / 1 |
| PKG_BLOCKED | package byte | 0 | any supported risk / boolean reversible |

One shared semantic validator serves scan/getter/export/order/lifecycle lookup
and append validation. Refused SUGGESTED is not a producer-supported event and
cannot suppress a legitimate suggestion. Auxiliary tags cannot turn a PANIC
tuple into CLEAN_BOOT authority.

Compatibility review must retain producer-valid historical records, including
failed APPLIED and refused PKG_ROLLED_BACK. Older synthetic tests that label
every event/flag cross-product valid must be replaced with producer-valid
fixtures, not used to widen authority. A rejected semantic record remains in
place and may be reported as corrupt/unsupported evidence, never reused.

## Verification and limits

Required tests include the preserved 115 checks, the latest audit's 23 cases
with explicit fixture adaptations if a check assumed weaker semantics, and new
actual-C/QEMU joint-state, candidate-write, staged-failure, chronology and ring
boundary controls. No test adaptation may grant permission by fallback or weaken
an evidence-preservation assertion. Report exact adaptations and before/after
outcomes. Physical torn-write/flush atomicity, hostile coherent metadata forgery
and arbitrary binary package execution remain outside the claimed guarantee.
