#!/usr/bin/env python3
"""Validate a KamikazeOS package manifest JSON file."""

from __future__ import annotations

import argparse
from pathlib import Path

from manifest_lib import TRUST_ANCHOR_FINGERPRINT, TRUST_ANCHOR_NAME, load_manifest, validate_manifest


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("manifest", type=Path)
    args = ap.parse_args()

    manifest = load_manifest(args.manifest)
    ok, errors, warnings = validate_manifest(manifest)
    print(f"id={manifest.package_id}")
    print(f"checksum={manifest.expected_checksum()}")
    print(f"trust_anchor={TRUST_ANCHOR_NAME}")
    print(f"fingerprint={TRUST_ANCHOR_FINGERPRINT}")
    for warning in warnings:
        print(f"warning: {warning}")
    if not ok:
        print("result=FAIL")
        for error in errors:
            print(f"error: {error}")
        return 1
    print("result=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
