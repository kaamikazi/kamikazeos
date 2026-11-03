# KamikazeOS Trust And Signatures

Status: v1.3 public release. The technical trust milestone is v1.2
"Trust Forge".

KamikazeOS v1.2 adds real host-side offline signing for package manifests.
The kernel still does not implement Ed25519 math; it enforces signature
metadata, checksums, trust-anchor identity, and package policy gates.

## Real

- Host-side Ed25519 keypair generation.
- Host-side deterministic signed payload.
- Host-side signature verification.
- Host-side signer authorization against a pinned KMZ-ROOT-1 public key.
- Import refusal for unsigned, invalid, or tampered manifests.
- Disk manifest import/export.
- Kernel-side independent checksum/trust/policy rejection.

## Simulated

- In-kernel Ed25519 verification.
- Binary patching.
- Actual module replacement.
- Internet update fetching.

## Tools

Generate a candidate offline signing key:

```powershell
python tools/package-builder/generate_keypair.py --out-dir build/keys --name kmz-root
```

Generated keys are not automatically trusted. The accepted KMZ-ROOT-1 public
key and SHA-256 fingerprint are pinned in
`tools/package-builder/trust_anchor.py`. The corresponding private key is not
stored in the repository.

Sign a manifest:

```powershell
python tools/package-builder/sign_manifest.py packages/examples/PKG-101-ghostattach-policy.json build/keys/kmz-root-private.pem
```

Signing is refused unless the private key matches the pinned public key.

Verify a signature:

```powershell
python tools/package-builder/verify_signature.py packages/examples/PKG-101-ghostattach-policy.json
```

Import a signed manifest:

```powershell
python tools/package-builder/import_manifest.py packages/examples/PKG-101-ghostattach-policy.json build/recovery.img --slot 0
```

## Signed Payload

The signed payload is deterministic JSON:

- sorted keys
- compact separators
- excludes `checksum`
- excludes `signature`

The manifest stores:

- `signature.algorithm = ed25519`
- `signature.key_id`
- `signature.public_key`
- `signature.public_key_sha256`
- `signature.payload_sha256`
- `signature.signature`

Changing any signed manifest field changes `payload_sha256` and makes host
verification fail.

## Kernel Boundary

The kernel reads compact signature metadata from the manifest sector:

- signature present marker
- signature algorithm
- public-key hash prefix
- signed-payload hash prefix

Because full Ed25519 is not implemented in the kernel, report fields say
`signature_mode=simulated_kernel_check_host_ed25519`. The host tools are still
responsible for real signature verification before import, and the kernel is
still responsible for rejecting unsafe manifests at boot.

## Public Demo Wording

Use this exact distinction when presenting the project:

- Real: offline Ed25519 signing and verification by host tools.
- Real: kernel-side checksum, trust-anchor, signature metadata, and policy
  enforcement.
- Simulated: in-kernel Ed25519 cryptographic verification.
