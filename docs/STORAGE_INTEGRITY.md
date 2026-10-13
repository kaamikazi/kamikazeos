# Recovery Store Integrity (v2.6.1 "Integrity Gate")

The recovery journal is evidence. v2.6.1 guarantees that no existing
journal entry is overwritten, whether the cause is a torn write, a
damaged state block, a full journal, or ambiguous metadata.

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

Every save writes the identical sector to **LBA 50 first, then LBA 1**.

## Choosing the trusted state copy at boot

| Primary (LBA 1) | Mirror (LBA 50) | Result |
|---|---|---|
| valid with extension | valid with extension, different generation | newer generation wins |
| valid with extension | valid, same generation, identical bytes | primary |
| valid with extension | valid, same generation, **different bytes** | **ambiguous: store read-only this boot** (`STORE-ERR-011`) |
| valid with extension | invalid or zero | primary |
| valid v1, no extension | anything | primary (a pre-2.6.1 kernel wrote it last) |
| extension magic but bad CRC, or garbage | valid with extension | mirror (`STORE-WARN-009`) |
| invalid | invalid | flags from CMOS, `STORE-ERR-010`; journal still usable |
| zero | zero | fresh store (`STORE-001`, `state_source=fresh`) |
| unreadable | unreadable | store read-only this boot (`STORE-ERR-012`) |

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
- When both state copies are lost, the boot number is floored at the
  highest boot number found in a valid journal entry, so new evidence is
  never filed under an old boot.
- An append also refuses to write a slot that is unexpectedly non-zero
  (`STORE-ERR-014`).
- A full journal refuses appends, warns **once per boot**
  (`STORE-WARN-004`), and counts refused appends in `journal_dropped`.

Nothing in the kernel erases history except the explicit, confirmed
`clear-store confirm` command.

## Report fields (`[RECOVERY_STORE]`)

`state_source`, `state_generation`, `state_redundancy=mirror_lba50`,
`state_locked`, `journal_locked`, `journal_capacity`, `journal_full`,
`journal_dropped`, `journal_corrupt_entries`. All previous fields are
unchanged.

## Compatibility (tested)

| Scenario | Result | Evidence |
|---|---|---|
| v2.6.1 boots an image last written by v2.6.0 | loads the v1 block, rebuilds the position, adds extension and mirror on the first save | `test_legacy_image_upgrades_in_place` |
| v2.6.0 boots an image written by v2.6.1 | reads LBA 1 as a valid v1 block, boot count continues | `test_downgrade_old_kernel_reads_new_image` |
| v2.6.1 boots again after v2.6.0 wrote | the legacy primary wins (newest writer) | same test, second half |

No migration step is required, and no existing sector changes meaning.
A v2.6.0 kernel still has the old overwrite behaviour if it is booted on
a damaged image. Upgrading is what removes it.
