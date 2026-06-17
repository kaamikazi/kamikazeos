#!/usr/bin/env python3
"""Verify a KamikazeOS manifest Ed25519 signature."""

from __future__ import annotations

import argparse
from pathlib import Path

from manifest_lib import load_manifest, validate_manifest, verify_manifest_signature


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("manifest", type=Path)
    ap.add_argument("--public-key", type=Path)
    args = ap.parse_args()

    manifest = load_manifest(args.manifest)
    public_key_pem = args.public_key.read_bytes() if args.public_key else None
    sig_ok, reason = verify_manifest_signature(manifest, public_key_pem)
    ok, errors, warnings = validate_manifest(manifest)
    print(f"id={manifest.package_id}")
    print(f"signature={reason}")
    print(f"payload_sha256={manifest.payload_sha256()}")
    for warning in warnings:
        print(f"warning: {warning}")
    if not sig_ok or not ok:
        print("result=FAIL")
        for error in errors:
            print(f"error: {error}")
        return 1
    print("result=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
