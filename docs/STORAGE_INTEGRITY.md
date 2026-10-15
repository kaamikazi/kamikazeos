# Recovery Store Integrity (v2.6.1 "Integrity Gate")

The recovery journal is evidence. v2.6.1 preserves occupied journal slots
under the tested torn-write, damaged-state, full-journal and ambiguous-state
scenarios. This is bounded recovery metadata, not a crash-safe filesystem.

## Disk layout (unchanged except LBA 50)

| LBA | Contents |
|---|---|
| 0 | untouched |
| 1 | recovery state block, v1 fields + v2.6.1 extension |
| 2-15 | append-only journal, 448 x 16-byte entries |
| 16-31 | boot snapshots |
| 32-39 | package manifests |
| 48-49 | package lifecycle state (primary, mirror) |
| **50** | **v2.6.1: full mirror copy of the LBA 1 sector** |
| 56 | module A/B slots |
| 64-127 | TinyFS |

## State block extension

Bytes 0-17 of LBA 1 are the unchanged v1 block (magic `KMZR`, version 1,
fields, and an additive checksum). v1 kernels always wrote bytes 18-511 as
zero and never read them. v2.6.1 uses bytes 32-51:

| Offset | Field |
|---|---|
| 32-35 | `KSX1` extension magic |
| 36 | extension version (1) |
| 37 | flags: bit 0 = last panic was an intentional test |
| 40-43 | generation, +1 on every save |
| 44-45 | copy of the journal count |
| 48-51 | CRC-32 (IEEE) over bytes 0-47 |

When capacity exceeds 50 sectors, a save writes the identical sector to
**LBA 50 first, then LBA 1**. When IDENTIFY proves that LBA 50 is absent,
only LBA 1 is read and written, with the same v1 fields and extension.
This is `state_redundancy=single_copy_capacity`; no redundancy is claimed.
An existing sector returning an I/O error locks the store. Absence and failed
I/O are deliberately distinct. Images below the complete journal range cannot
establish a safe append position and remain locked.

## Choosing the trusted state copy at boot

| Primary (LBA 1) | Mirror (LBA 50) | Result |
|---|---|---|
| valid with extension | valid with extension, different generation | newer generation wins |
| valid with extension | valid, same generation, identical bytes | primary |
| valid with extension | valid, same generation, **different bytes** | **ambiguous: store read-only this boot** (`STORE-ERR-011`) |
| valid with extension | valid, generation distance exactly `0x80000000` | **unordered: store read-only this boot** (`STORE-ERR-011`) |
| valid with extension | invalid or zero | primary |
| valid v1, entire reserved tail zero | readable | primary supplies flags; retain a valid mirror's generation as the floor for the next write |
| extension magic but bad CRC, or garbage | valid with extension | mirror (`STORE-WARN-009`) |
| invalid | invalid | flags from CMOS, `STORE-ERR-010`; journal still usable |
| zero | zero | fresh store (`STORE-001`, `state_source=fresh`) |
| either existing copy unreadable | any | store read-only this boot (`STORE-ERR-012`) |

Legacy recognition requires bytes 18-511 to be zero, matching actual legacy
writers. A missing or damaged extension magic with a nonzero tail is damaged
modern state, not permission to bypass the CRC through the v1 checksum.

Recovery and package state share unsigned serial-number comparison: equality,
newer, older, and the unordered half-range case are distinct. Normal wrap from
`0xffffffff` to zero is supported. Package copies at LBA 48/49 also hold lifecycle
writes (`PST-ERR-002`) on half-range separation or equal-generation differing
sectors. No copy is hydrated on ambiguity. A refused package-state reset cannot
proceed to reset module slots. No sector layout or checksum definition changes.

## Journal append position

The position is rebuilt from the journal itself at every boot:

- It is one past the **last occupied (non-zero) slot**. Valid, torn, and
  corrupt slots all count as occupied and are never reused.
- It is never below the count recorded in a trusted state copy. If the
  state claims more entries than the disk shows (`STORE-WARN-010`), the
  claimed slots stay reserved.
- If it is above the recorded count (a torn append, or lost state), it is
  logged as `STORE-WARN-008`. Corrupt entries are reported as
  `STORE-WARN-011` and preserved.
- Readable, version-valid journal entries establish a diagnostic boot-number
  lower bound even when state is locked. A complete scan is required before
  persistent ordering is trusted; unreadable sectors never grant permission
  to append. Valid selected state can raise the lower bound.
- An append also refuses to write a slot that is unexpectedly non-zero
  (`STORE-ERR-014`).
- A full journal refuses appends, warns **once per boot**
  (`STORE-WARN-004`), and counts refused appends in `journal_dropped`.

Scanning, exported journal/package history, and latest-action lookups share
the same validator: magic, supported version, checksum and recognized event
range must all pass. Result and reversible are 0/1, risk is low/medium/high,
non-package action IDs are 0-4 or 255, the reserved byte is zero, and the
six-byte tag is printable ASCII with zero padding after its first NUL.
Package event IDs retain the full byte range. An occupied unsupported-version or unknown-event record
is retained as evidence but cannot supply boot ordering, appear as a valid
event, or suppress a newly changed recovery suggestion.

Nothing in the kernel erases history except the explicit, confirmed
`clear-store confirm` command.

## Snapshot evidence and ordering

`boot_snapshot_save()` refuses writes when state or journal is locked, the
journal scan is incomplete, state persistence failed, or `boot_order_trusted`
is false. It reads the full snapshot area before choosing a slot. Unreadable,
nonzero corrupt, or future/same-boot snapshot records hold writes with
`SNAP-WARN-002`. Existing sectors are preserved byte-for-byte in the regression
fixtures. Healthy boot history retains the existing 16-slot ring behavior.

A high boot lower bound alone does not prove that history aged through every
intervening boot. After history exists, snapshot progression must be consecutive
or corroborated by a successful, fully validated CLEAN_BOOT record for each
intervening boot. Gaps without that evidence hold writes, preserving all slots;
healthy 16-slot rotation and corroborated gaps remain allowed. This establishes
format consistency, not authentication of checksummed disk contents.

Every occupied snapshot must also be at `LBA 16 + boot % 16`, unique and older
than the current boot. The actual destination must be empty or at least 16
boots old. Global maximum age never substitutes for victim-specific eligibility.
Misplaced/duplicate sectors are held unchanged, not relocated or repaired.

Boot numbers remain one-byte fields. At 255, the next boot holds durable
state, journal and snapshot writes with `STORE-ERR-015`; it does not wrap to
zero. Widening the format requires a separately reviewed migration. CMOS
remains a diagnostic fallback and does not unlock uncertain disk ordering.

## Package rollback ownership

The package-state v1 `slot_before_apply` field already supports `SLOT_NONE`.
When an apply commits, the same mirrored package-state save that publishes
the new owner sets this field to `SLOT_NONE` in superseded records for that
module. Status, versions, manifest binding and journal history are retained.
No new fields, sector owners, format versions or migration are required.

Staging leaves prior ownership intact. After interruption, an aborted apply
keeps that ownership; a completed slot switch publishes the new owner and
retirements together through the existing transaction reconciliation. A failed
package commit restores all affected in-memory records, retaining the staged
intent. Further lifecycle operations on that module are refused until its
transaction is reconciled. No module slot is automatically switched by recovery.

For older images with indistinguishable claims to the same slot/version
transition, every competing rollback is refused (`PKG-REJECT-001`). The kernel
does not guess which claimant is newest. A rollback of a new owner never
restores superseded rollback rights. Older kernels remain subject to their own
rollback-policy defects; format readability does not imply policy equivalence.

### Package-copy trust after a failure

Package ownership no longer falls back to a lone readable/valid copy at boot.
Both copies must be readable, format-valid and unambiguously ordered. A
nonzero damaged sidecar cannot be reinterpreted as legacy base state. Missing,
unreadable, damaged or unbound legacy ownership is held without hydration,
reconciliation or repair. Approve/apply/verify/rollback and reset cannot write
the questionable package/slot evidence. Refused lifecycle commands emit
`PKG-REJECT-003` with the reason; no refused package event is persisted.

`[PACKAGE_STATE_INTEGRITY]` reports `ownership_state`, `ownership_reason` and
`writes_allowed`. These are RAM/report fields, not a format extension:

| State | Permission |
|---|---|
| `fresh` | Both package-state sectors and the module-slot sector are empty; explicit lifecycle commands may establish state |
| `verified` | Both copies are readable, valid and ordered, or both current writes succeeded |
| `degraded_current` | This session knows its requested transition reached one durable copy; runtime ownership remains current; reboot must revalidate both copies |
| `uncertain` | No ownership-dependent writes, automatic reconciliation, legacy binding or reset |
| `unavailable` | No disk or insufficient capacity for package/slot storage; writes held |

Both failed saves hold subsequent operations even when RAM snapshots were
restored. A cleared transient read fault can restore trust on reboot when both
current copies validate; a missing or damaged copy is not automatically rebuilt.
Conservative holds trade availability for evidence preservation. Legacy package
bytes remain format-compatible, but old single-copy/unbound metadata is no longer
automatically promoted to mutation authority. The tagged v2.6.0 transaction
format and its tested downgrade/re-upgrade remain compatible. No migration,
record version, checksum coverage or LBA owner changes.

## Report fields (`[RECOVERY_STORE]`)

`state_source`, `state_generation`, `state_redundancy`,
`state_locked`, `journal_locked`, `journal_capacity`, `journal_full`,
`journal_dropped`, `journal_corrupt_entries`, `journal_scan_valid`,
`boot_order_trusted`. All previous fields are
unchanged.

## Compatibility (tested)

| Scenario | Result | Evidence |
|---|---|---|
| v2.6.1 boots an image last written by v2.6.0 | loads the v1 block, rebuilds the position, adds extension and mirror on the first save | `test_legacy_image_upgrades_in_place` |
| v2.6.0 boots an image written by v2.6.1 | reads LBA 1 as a valid v1 block, boot count continues | `test_downgrade_old_kernel_reads_new_image` |
| v2.6.1 boots again after v2.6.0 wrote | the legacy primary wins (newest writer) | same test, second half |

No migration step is required, and no existing sector changes meaning.
The correction tests cover 32-, 50-, 51- and 2048-sector disks, including
full QEMU restarts, legacy state, unchanged journal prefixes and image sizes.
A v2.6.0 kernel still has the old overwrite behaviour if it is booted on
a damaged image. Upgrading is what removes it.
