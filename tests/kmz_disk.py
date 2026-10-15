"""Host-side helpers for KamikazeOS disposable test disk images.

These helpers mirror the kernel's on-disk formats so regression tests can
inspect images after a boot and inject failures (torn writes, bad
checksums, full journals) into DISPOSABLE copies. They never touch
build/recovery.img: every test creates its own image under its run dir.

Layout (LBA = 512-byte sector):
  1        recovery state block (primary)
  2..15    append-only recovery journal (16-byte entries)
  16..31   boot snapshots
  32..39   package manifests
  48..49   package lifecycle state (primary, mirror)
  50       recovery state block mirror (v2.6.1+)
  56       module A/B slots
  64..127  TinyFS
"""

from __future__ import annotations

import sys
from pathlib import Path

SECTOR = 512
IMAGE_SECTORS = 2048

LBA_STATE = 1
LBA_JOURNAL = 2
JOURNAL_SECTORS = 14
ENTRY_SIZE = 16
ENTRIES_PER_SECTOR = SECTOR // ENTRY_SIZE
JOURNAL_CAPACITY = JOURNAL_SECTORS * ENTRIES_PER_SECTOR
LBA_STATE_MIRROR = 50
LBA_MANIFESTS = 32
LBA_SLOTS = 56

JE_MAGIC = 0xA7
JE = {
    "SUGGESTED": 1, "APPROVED": 2, "APPLIED": 3, "ROLLED_BACK": 4,
    "BLOCKED": 5, "PANIC": 6, "SAFE_MODE_BOOT": 7, "CLEAN_BOOT": 8,
    "PKG_SUGGESTED": 9, "PKG_APPROVED": 10, "PKG_APPLIED": 11,
    "PKG_VERIFIED": 12, "PKG_FAILED": 13, "PKG_ROLLED_BACK": 14,
    "PKG_BLOCKED": 15,
}

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "package-builder"))


def blank_image(path: Path, sectors: int = IMAGE_SECTORS) -> Path:
    path.write_bytes(b"\0" * (SECTOR * sectors))
    return path


def read_sector(path: Path, lba: int) -> bytes:
    with path.open("rb") as f:
        f.seek(lba * SECTOR)
        return f.read(SECTOR)


def write_sector(path: Path, lba: int, data: bytes) -> None:
    assert len(data) == SECTOR
    with path.open("r+b") as f:
        f.seek(lba * SECTOR)
        f.write(data)


def sum_byte(data: bytes) -> int:
    return (-sum(data)) & 0xFF


# ---------------------------------------------------------------- journal

def make_entry(boot: int, event: int, action: int = 0, result: int = 1,
               risk: int = 0, reversible: int = 1, code: str = "TEST01") -> bytes:
    e = bytearray(ENTRY_SIZE)
    e[0] = JE_MAGIC
    e[1] = 1
    e[2] = boot & 0xFF
    e[3] = event
    e[4] = action
    e[5] = result
    e[6] = risk
    e[7] = reversible
    raw = code.encode("ascii")[:6]
    e[8:8 + len(raw)] = raw
    e[15] = sum_byte(e[:15])
    return bytes(e)


def journal_slot(path: Path, index: int) -> bytes:
    sec = read_sector(path, LBA_JOURNAL + index // ENTRIES_PER_SECTOR)
    off = (index % ENTRIES_PER_SECTOR) * ENTRY_SIZE
    return sec[off:off + ENTRY_SIZE]


def write_journal_slot(path: Path, index: int, entry: bytes) -> None:
    lba = LBA_JOURNAL + index // ENTRIES_PER_SECTOR
    sec = bytearray(read_sector(path, lba))
    off = (index % ENTRIES_PER_SECTOR) * ENTRY_SIZE
    sec[off:off + ENTRY_SIZE] = entry
    write_sector(path, lba, bytes(sec))


def journal_bytes(path: Path) -> bytes:
    with path.open("rb") as f:
        f.seek(LBA_JOURNAL * SECTOR)
        return f.read(JOURNAL_SECTORS * SECTOR)


def entry_valid(entry: bytes) -> bool:
    if not (len(entry) == ENTRY_SIZE and entry[0] == JE_MAGIC and entry[1] == 1
            and JE["SUGGESTED"] <= entry[3] <= JE["PKG_BLOCKED"]
            and entry[5] <= 1 and entry[6] <= 2 and entry[7] <= 1 and entry[14] == 0
            and (entry[3] >= JE["PKG_SUGGESTED"] or entry[4] <= 4 or entry[4] == 255)
            and (sum(entry) & 0xFF) == 0):
        return False
    code = entry[8:14].split(b"\0", 1)[0]
    return all(32 <= c <= 126 for c in code) and not any(entry[8 + len(code):14])


def journal_entries(path: Path) -> list[bytes]:
    """All occupied (non-zero) 16-byte slots, in order."""
    blob = journal_bytes(path)
    out = []
    for i in range(JOURNAL_CAPACITY):
        e = blob[i * ENTRY_SIZE:(i + 1) * ENTRY_SIZE]
        if any(e):
            out.append(e)
    return out


def journal_used_slots(path: Path) -> int:
    """1 + index of the last non-zero journal slot (0 when empty)."""
    blob = journal_bytes(path)
    last = -1
    for i in range(JOURNAL_CAPACITY):
        if any(blob[i * ENTRY_SIZE:(i + 1) * ENTRY_SIZE]):
            last = i
    return last + 1


def decode_entry(e: bytes) -> dict:
    inv = {v: k for k, v in JE.items()}
    return {"boot": e[2], "event": inv.get(e[3], e[3]), "action": e[4],
            "result": e[5], "code": e[8:14].rstrip(b"\0").decode("ascii", "replace"),
            "valid": entry_valid(e)}


def fill_journal(path: Path, count: int, boot: int = 1) -> None:
    """Write `count` valid filler entries from slot 0 (whole sectors)."""
    for i in range(count):
        write_journal_slot(path, i, make_entry(boot, JE["CLEAN_BOOT"], code="FILLER"))


# ------------------------------------------------------------- state block

def state_fields(path: Path, lba: int = LBA_STATE) -> dict:
    s = read_sector(path, lba)
    return {
        "magic_ok": s[0:4] == b"KMZR",
        "version": s[4],
        "boot_count": s[5],
        "panic": s[7],
        "jcount": s[15] | (s[16] << 8),
        "v1_checksum_ok": (sum(s[0:18]) & 0xFF) == 0,
        "raw": s,
    }


def write_legacy_state(path: Path, boot_count: int, jcount: int,
                       panic: int = 0) -> None:
    """A v1 (pre-2.6.1) state block: no generation extension, no mirror."""
    s = bytearray(SECTOR)
    s[0:4] = b"KMZR"
    s[4] = 1
    s[5] = boot_count & 0xFF
    s[6] = 0 if panic else boot_count & 0xFF
    s[7] = panic
    s[15] = jcount & 0xFF
    s[16] = (jcount >> 8) & 0xFF
    s[17] = sum_byte(s[0:17])
    write_sector(path, LBA_STATE, bytes(s))


# v2.6.1 generation extension: lives in LBA 1's reserved bytes (old kernels
# ignore them) and the whole sector is mirrored at LBA 50.
EXT_OFF = 32
EXT_MAGIC = b"KSX1"
EXT_CRC_OFF = 48


def _crc32(data: bytes) -> int:
    import zlib
    return zlib.crc32(data) & 0xFFFFFFFF


def state_ext_valid(sector: bytes) -> bool:
    if sector[0:4] != b"KMZR" or (sum(sector[0:18]) & 0xFF) != 0:
        return False
    if sector[EXT_OFF:EXT_OFF + 4] != EXT_MAGIC or sector[EXT_OFF + 4] != 1:
        return False
    stored = int.from_bytes(sector[EXT_CRC_OFF:EXT_CRC_OFF + 4], "little")
    return stored == _crc32(sector[0:EXT_CRC_OFF])


def state_generation(sector: bytes) -> int:
    return int.from_bytes(sector[EXT_OFF + 8:EXT_OFF + 12], "little")


def resign_state(sector: bytes, boot_count: int | None = None,
                 generation: int | None = None) -> bytes:
    """Edit a v2.6.1 state sector and re-seal BOTH checksums, keeping its
    generation: produces an internally valid but conflicting copy."""
    s = bytearray(sector)
    if boot_count is not None:
        s[5] = boot_count & 0xFF
    if generation is not None:
        s[EXT_OFF + 8:EXT_OFF + 12] = generation.to_bytes(4, "little")
    s[17] = sum_byte(s[0:17])
    s[EXT_CRC_OFF:EXT_CRC_OFF + 4] = _crc32(bytes(s[0:EXT_CRC_OFF])).to_bytes(4, "little")
    return bytes(s)


def corrupt_byte(path: Path, lba: int, offset: int, xor: int = 0x5A) -> None:
    sec = bytearray(read_sector(path, lba))
    sec[offset] ^= xor
    write_sector(path, lba, bytes(sec))


# --------------------------------------------------------------- manifests

def fixture_manifest_sector(package_id: str, name: str, description: str,
                            target_component: str, from_version: str,
                            to_version: str, target_module: str = "GhostAttach",
                            key_hash_prefix: str | None = None,
                            signature_status: str = "trusted",
                            marker: int = 2) -> bytes:
    """Build a disk manifest sector for a TEST FIXTURE.

    Real imports go through import_manifest.py, which writes the
    host-verified marker (signature_present=2) only after a real Ed25519
    check against the pinned KMZ-ROOT-1 key. The kernel's own check of that
    marker is SIMULATED (documented in docs/TRUST_AND_SIGNATURES.md). Tests
    need chained same-module updates that no real signed example provides,
    so this helper writes the marker directly into a disposable image to
    stand in for "a manifest the host tooling already verified". It is a
    test fixture of the simulated boundary, not a signature.
    """
    from manifest_lib import Manifest, manifest_to_sector, O_SIG_PRESENT
    from trust_anchor import TRUSTED_PUBLIC_KEY_SHA256

    m = Manifest.from_json({
        "package_id": package_id, "name": name, "description": description,
        "target_component": target_component, "target_module": target_module,
        "from_version": from_version,
        "to_version": to_version, "risk": "low", "reversible": True,
        "requires_approval": True, "signature_status": signature_status,
        "trust_anchor_id": 1, "manifest_version": 1,
        "payload_type": "module_slot_switch_simulated",
        "signature": {
            "algorithm": "ed25519", "key_id": "TEST-FIXTURE",
            "public_key_sha256": key_hash_prefix or TRUSTED_PUBLIC_KEY_SHA256,
            "payload_sha256": "0" * 64, "signature": "00",
        },
    })
    sec = bytearray(manifest_to_sector(m))
    sec[O_SIG_PRESENT] = marker
    return bytes(sec)


def write_manifest(path: Path, slot: int, sector: bytes) -> None:
    write_sector(path, LBA_MANIFESTS + slot, sector)


def slots_sector(path: Path) -> bytes:
    return read_sector(path, LBA_SLOTS)


def resign_package_state(sector: bytes, generation: int) -> bytes:
    """Reseal existing v1 lifecycle records and their transaction sidecar."""
    s = bytearray(sector)
    count = s[5]
    used = 6 + count * 37
    s[used] = sum_byte(s[:used])
    s[326:330] = generation.to_bytes(4, "little")
    tx_used = 320 + 10 + count * 8
    s[tx_used] = sum_byte(s[320:tx_used])
    return bytes(s)
