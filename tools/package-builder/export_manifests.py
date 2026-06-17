#!/usr/bin/env python3
"""Export package manifests from build/recovery.img manifest sectors."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from manifest_lib import LBA_MANIFESTS, MANIFEST_MAX, read_manifest_slots, validate_manifest


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("image", type=Path, nargs="?", default=Path("build/recovery.img"))
    ap.add_argument("--out", type=Path, help="optional JSON output file")
    args = ap.parse_args()

    if not args.image.exists():
        print(f"error: disk image not found: {args.image}")
        return 2

    rows = []
    for slot, manifest in enumerate(read_manifest_slots(args.image)):
        if manifest is None:
            continue
        ok, errors, warnings = validate_manifest(manifest)
        rows.append(
            {
                "slot": slot,
                "lba": LBA_MANIFESTS + slot,
                "valid": ok,
                "errors": errors,
                "warnings": warnings,
                "manifest": manifest.to_json(),
            }
        )

    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(json.dumps(rows, indent=2) + "\n")
        print(f"wrote {args.out}")
    else:
        print(json.dumps(rows, indent=2))
    print(f"exported={len(rows)}")
    print(f"capacity={MANIFEST_MAX}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
