# KamikazeOS Package Manifest Schema

Status: v1.3 public release. The manifest tooling was introduced in v1.1 and
signed in v1.2.

KamikazeOS package manifests are small JSON documents on the host and fixed
512-byte sectors on `build/recovery.img`. Host tools can create, validate,
import, and export them, but the kernel still validates every manifest at boot.

## Real

- Offline manifest tooling.
- Deterministic manifest checksums.
- Real host-side Ed25519 manifest signatures.
- Disk manifest import/export.
- Kernel-side manifest validation.
- Package policy rejection.

## Simulated

- Cryptographic signature verification.
- In-kernel Ed25519 verification.
- Binary patching.
- Actual module replacement.
- Internet update fetching.

## Disk Layout

The host tools write only package manifest sectors:

| LBA | Purpose |
|---:|---|
| 32-39 | Package manifest slots, 8 sectors total |

The tools must not touch LBA 0, RecoveryStore state, journal, snapshots,
package states, or module slots.

## JSON Fields

```json
{
  "package_id": "PKG-101",
  "name": "ghostattach_policy_import",
  "description": "Import GhostAttach policy 0.3 to 0.4",
  "target_component": "GhostAttach",
  "target_module": "GhostAttach",
  "from_version": "0.3",
  "to_version": "0.4",
  "risk": "low",
  "reversible": true,
  "requires_approval": true,
  "signature_status": "signed_placeholder",
  "trust_anchor_id": 1,
  "manifest_version": 1,
  "payload_type": "module_slot_switch_simulated",
  "checksum": 181,
  "signature": {
    "algorithm": "ed25519",
    "key_id": "KMZ-ROOT-1",
    "public_key": "...hex...",
    "public_key_sha256": "...hex...",
    "payload_sha256": "...hex...",
    "signature": "...hex..."
  }
}
```

Field limits mirror the kernel sector format:

All text fields must be printable ASCII and may not contain `;`, `=`, or
control characters because those bytes delimit the serial report contract.
Unknown top-level JSON fields are rejected rather than silently left unsigned.
`reversible` and `requires_approval` must be JSON booleans, and every accepted
package must set `requires_approval` to `true`.

| Field | Limit / Values |
|---|---|
| `package_id` | ASCII, max 9 bytes, `PKG-` plus decimal digits |
| `name` | ASCII, max 31 bytes |
| `description` | ASCII, max 47 bytes |
| `target_component` | ASCII, max 19 bytes |
| `target_module` | `Kernel`, `GhostAttach`, `RecoveryEngine`, `RecoveryMemory`, `RecoveryStore`, `AI Recovery Core Rules`, `Recovery Center Schema` |
| `from_version`, `to_version` | ASCII, max 7 bytes each |
| `risk` | `low`, `medium`, `high` |
| `signature_status` | `trusted` for imported manifests; built-in seeds may use `signed_placeholder` |
| `trust_anchor_id` | `1` for `KMZ-ROOT-1` |
| `payload_type` | `metadata_only`, `module_slot_switch_simulated`, `blocked_driver_install`, `unknown` |

## Policy

A manifest is blocked if any of these are true:

- checksum mismatch
- unsigned, placeholder, invalid, missing, or tampered signature for imported manifests
- unknown trust anchor
- high risk
- irreversible
- blocked or unknown payload type
- unknown target module

Host tools perform real Ed25519 verification before import. The kernel stores
and reports compact signature metadata, but in-kernel Ed25519 verification is
still simulated in v1.3.

## Tools

```powershell
python tools/package-builder/validate_manifest.py packages/examples/PKG-101-ghostattach-policy.json
python tools/package-builder/generate_keypair.py --out-dir build/keys --name kmz-root
python tools/package-builder/sign_manifest.py packages/examples/PKG-101-ghostattach-policy.json build/keys/kmz-root-private.pem
python tools/package-builder/verify_signature.py packages/examples/PKG-101-ghostattach-policy.json
python tools/package-builder/import_manifest.py packages/examples/PKG-101-ghostattach-policy.json build/recovery.img
python tools/package-builder/export_manifests.py build/recovery.img --out build/exported-manifests.json
```

Unsafe manifests are refused by default. For canary testing only:

```powershell
python tools/package-builder/import_manifest.py packages/examples/PKG-199-blocked-driver-canary.json build/recovery.img --unsafe-test
```

The kernel must still block unsafe manifests even if the host tool writes them.
