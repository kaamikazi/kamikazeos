#!/usr/bin/env python3
"""Create a KamikazeOS package manifest JSON file."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from manifest_lib import (
    PAYLOADS,
    RISKS,
    SIGNATURES,
    TRUST_ANCHOR_ID,
    Manifest,
    validate_manifest,
)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("output", type=Path)
    ap.add_argument("--id", required=True, dest="package_id")
    ap.add_argument("--name", required=True)
    ap.add_argument("--description", required=True)
    ap.add_argument("--target-component", required=True)
    ap.add_argument("--target-module", required=True)
    ap.add_argument("--from-version", required=True)
    ap.add_argument("--to-version", required=True)
    ap.add_argument("--risk", choices=RISKS, default="low")
    ap.add_argument("--payload-type", choices=PAYLOADS, default="module_slot_switch_simulated")
    ap.add_argument("--signature-status", choices=SIGNATURES, default="signed_placeholder")
    ap.add_argument("--trust-anchor-id", type=int, default=TRUST_ANCHOR_ID)
    ap.add_argument("--manifest-version", type=int, default=1)
    ap.add_argument("--irreversible", action="store_true")
    ap.add_argument("--no-approval", action="store_true")
    args = ap.parse_args()

    manifest = Manifest(
        package_id=args.package_id,
        name=args.name,
        description=args.description,
        target_component=args.target_component,
        target_module=args.target_module,
        from_version=args.from_version,
        to_version=args.to_version,
        risk=args.risk,
        reversible=not args.irreversible,
        requires_approval=not args.no_approval,
        signature_status=args.signature_status,
        trust_anchor_id=args.trust_anchor_id,
        manifest_version=args.manifest_version,
        payload_type=args.payload_type,
    )

    ok, errors, warnings = validate_manifest(manifest)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(manifest.to_json(), indent=2) + "\n")
    for warning in warnings:
        print(f"warning: {warning}")
    if not ok:
        print(f"wrote {args.output} (UNSAFE/INVALID)")
        for error in errors:
            print(f"error: {error}")
        return 1
    print(f"wrote {args.output}")
    print(f"checksum={manifest.expected_checksum()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
