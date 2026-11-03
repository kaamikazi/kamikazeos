# TinyFS

KamikazeOS v1.8.0 "TinyFS" adds a deliberately small file abstraction over
the existing ATA PIO recovery disk. It is intended for bounded text storage and
filesystem learning, not general-purpose or production data.

## Fixed Disk Layout

TinyFS owns only LBA 64 through 127:

| LBA range | Purpose |
|---:|---|
| 64 | Checksummed TinyFS superblock |
| 65-72 | Serialized fixed file table |
| 73-127 | File data sectors |

All TinyFS sector I/O passes through a range-checking wrapper. Formatting loops
only across LBA 64-127. LBA 0, RecoveryStore state, journal, snapshots,
manifests, package states, and module slots are outside the accepted range.

## Format

The superblock records a magic value, format version, checksum, file count,
data start, maximum files, maximum file size, table location, and data end.

The file table contains up to 16 fixed entries. Each entry records:

- used flag and fixed flags
- filename up to 24 characters
- byte size
- starting LBA and sector count
- deterministic FNV-1a content checksum

Names may contain ASCII letters, digits, `.`, `_`, and `-`. There are no path
separators or directories.

## Allocation And Bounds

Files are limited to 2048 bytes or four sectors. Writes build a fixed 55-byte
sector-use map and choose the first contiguous free extent. Replacements keep
the current extent reserved while writing and reading back a distinct extent,
then commit the file-table entry before reclaiming the old extent. No heap
allocation is used. Reads verify the stored content checksum before returning
text.

## Commands

```text
fs
fs-format confirm
ls
touch note.txt
write note.txt hello kamikaze
cat note.txt
rm note.txt
```

`fs-format` requires the literal `confirm` argument. `write` accepts the rest
of the shell line as text, including spaces, within the shell and file limits.

## Failure Behavior

If the ATA disk is missing, TinyFS reports unavailable and the rest of the
kernel continues booting. Invalid metadata, names, sizes, extents, duplicate
entries, overlap, read failures, write failures, and checksum mismatches are
rejected and logged.

Deletion removes the file-table entry before optionally clearing the old data
sectors. If a metadata write may have completed only partially, TinyFS disables
further operations until the next boot revalidates the superblock and table.
These ordering rules protect the last referenced data during normal write
failures, but they do not make multi-sector metadata commits atomic. TinyFS is
still not a transactional or journaled filesystem.

## Not Implemented

- directories or path traversal
- permissions, users, or userspace files
- dynamic allocation or file growth beyond four sectors
- filesystem journaling or transactions
- atomic rename or crash-safe metadata updates
- sparse files, links, devices, or executable files

TinyFS stores text bytes only. It does not load or execute file contents.
