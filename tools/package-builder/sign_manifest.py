#!/usr/bin/env python3
"""Sign a KamikazeOS manifest with an offline Ed25519 private key."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

from cryptography.hazmat.primitives import serialization

from manifest_lib import TRUST_ANCHOR_ID, Manifest, load_manifest, validate_manifest


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("manifest", type=Path)
    ap.add_argument("private_key", type=Path)
    ap.add_argument("--out", type=Path)
    ap.add_argument("--key-id", default="KMZ-ROOT-1")
    args = ap.parse_args()

    manifest = load_manifest(args.manifest)
    private_key = serialization.load_pem_private_key(args.private_key.read_bytes(), password=None)
    public_key = private_key.public_key()
    public_raw = public_key.public_bytes(
        encoding=serialization.Encoding.Raw,
        format=serialization.PublicFormat.Raw,
    )

    manifest.signature_status = "trusted"
    manifest.trust_anchor_id = TRUST_ANCHOR_ID
    manifest.signature = None
    signature = private_key.sign(manifest.signed_payload())
    manifest.signature = {
        "algorithm": "ed25519",
        "key_id": args.key_id,
        "public_key": public_raw.hex(),
        "public_key_sha256": hashlib.sha256(public_raw).hexdigest(),
        "payload_sha256": manifest.payload_sha256(),
        "signature": signature.hex(),
    }
    manifest.checksum = manifest.expected_checksum()

    ok, errors, warnings = validate_manifest(manifest)
    for warning in warnings:
        print(f"warning: {warning}")
    if not ok:
        for error in errors:
            print(f"error: {error}")
        return 1

    out = args.out or args.manifest
    out.write_text(json.dumps(manifest.to_json(), indent=2) + "\n")
    print(f"signed {manifest.package_id}")
    print(f"wrote {out}")
    print(f"payload_sha256={manifest.payload_sha256()}")
    print(f"public_key_sha256={manifest.signature['public_key_sha256']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
