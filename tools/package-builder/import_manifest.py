#!/usr/bin/env python3
"""Import one package manifest into build/recovery.img manifest sectors."""

from __future__ import annotations

import argparse
from pathlib import Path

from manifest_lib import (
    LBA_MANIFESTS,
    MANIFEST_LBA_END,
    MANIFEST_MAX,
    load_manifest,
    read_manifest_slots,
    validate_manifest,
    write_manifest_slot,
)


def choose_slot(slots, requested: int | None) -> int:
    if requested is not None:
        if requested < 0 or requested >= MANIFEST_MAX:
            raise ValueError(f"slot must be 0..{MANIFEST_MAX - 1}")
        return requested
    for i, existing in enumerate(slots):
        if existing is None:
            return i
    raise ValueError("no empty manifest slot; pass --slot to replace one")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("manifest", type=Path)
    ap.add_argument("image", type=Path, nargs="?", default=Path("build/recovery.img"))
    ap.add_argument("--slot", type=int)
    ap.add_argument(
        "--unsafe-test",
        action="store_true",
        help="allow writing an unsafe manifest for kernel-side canary testing",
    )
    args = ap.parse_args()

    if not args.image.exists():
        print(f"error: disk image not found: {args.image}")
        return 2

    manifest = load_manifest(args.manifest)
    ok, errors, warnings = validate_manifest(manifest)
    for warning in warnings:
        print(f"warning: {warning}")
    if not ok and not args.unsafe_test:
        print("refused: manifest is unsafe or invalid")
        for error in errors:
            print(f"error: {error}")
        print("use --unsafe-test only for blocked-canary kernel tests")
        return 1

    slots = read_manifest_slots(args.image)
    slot = choose_slot(slots, args.slot)
    write_manifest_slot(args.image, slot, manifest)
    print(f"imported {manifest.package_id} into {args.image}")
    print(f"slot={slot}")
    print(f"lba={LBA_MANIFESTS + slot}")
    print(f"allowed_range=LBA {LBA_MANIFESTS}-{MANIFEST_LBA_END}")
    if not ok:
        print("unsafe-test=yes; kernel must still block this manifest")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
