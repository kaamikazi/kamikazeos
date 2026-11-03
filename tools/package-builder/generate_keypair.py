#!/usr/bin/env python3
"""Generate an Ed25519 keypair for offline KamikazeOS manifest signing."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path

from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--out-dir", type=Path, default=Path("build/keys"))
    ap.add_argument("--name", default="kmz-root")
    args = ap.parse_args()

    args.out_dir.mkdir(parents=True, exist_ok=True)
    private_key = Ed25519PrivateKey.generate()
    public_key = private_key.public_key()

    private_pem = private_key.private_bytes(
        encoding=serialization.Encoding.PEM,
        format=serialization.PrivateFormat.PKCS8,
        encryption_algorithm=serialization.NoEncryption(),
    )
    public_pem = public_key.public_bytes(
        encoding=serialization.Encoding.PEM,
        format=serialization.PublicFormat.SubjectPublicKeyInfo,
    )
    public_raw = public_key.public_bytes(
        encoding=serialization.Encoding.Raw,
        format=serialization.PublicFormat.Raw,
    )

    priv_path = args.out_dir / f"{args.name}-private.pem"
    pub_path = args.out_dir / f"{args.name}-public.pem"
    priv_path.write_bytes(private_pem)
    pub_path.write_bytes(public_pem)
    print(f"wrote {priv_path}")
    print(f"wrote {pub_path}")
    print(f"public_key_sha256={hashlib.sha256(public_raw).hexdigest()}")
    print("warning: keep the private key offline; do not import it into KamikazeOS")
    print("warning: this is a candidate key and is not trusted until explicitly pinned")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
