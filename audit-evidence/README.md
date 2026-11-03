# Preserved independent v2.6.1 release audit

Original audit target: `0cd86d88a1cc2a57b6a13f8f0e8d003dadf85f59`.
Verdict: **REVISE**. These are the original reviewer's observations, not a
claim of acceptance of later implementing-agent corrections.

The complete original temporary audit directory was preserved locally as
`audit-evidence/release-0cd86d8.zip` before correction work. The archive contains
**1,166 files**, **134,947,572 uncompressed bytes**, including independent source
exports, build artifacts, adversarial tests, disposable images, serial logs,
snapshot before/after bytes and reports. Every archived entry was verified
against its original SHA-256. Its checksum is:

```text
bae909075761af2c3edafe33e993002487d79cc61c9c0dae483ffccb2b75be75
```

The ZIP and full per-file `inventory.json` are local preserved artifacts and
ignored by Git. `release-0cd86d8.sha256` records the archive checksum. The original
`AUDIT_REPORT.md`, `audit-evidence.json`, `provenance.json` and `host-results.json`
are also retained as readable tracked copies. No temporary evidence was deleted.

Correction implementation evidence is in `docs/RELEASE_BLOCKERS.md`.

## Newest review of 8e75093

Its complete temporary directory is also preserved as
`audit-evidence/release-8e75093.zip`: 2,410 files / 289,238,824 uncompressed bytes,
each verified against its original SHA-256. Archive SHA-256:

```text
f5982850ab1d46a4ca4f0d9bdb09fab31311abc9e66f64387848ae8a10235365
```

The checksum file is tracked. Readable reports/provenance are under `8e75093/`;
the full local inventory is `inventory-8e75093.json` (ignored with the ZIP).
The earlier archive and both original temporary evidence directories are
unchanged. New correction evidence is in `docs/TRUST_CORRECTIONS.md`.

## Stage 2B Design And Review Publication

[Stage 2B archive](stage2b/README.md) preserves sanitized copies of the design,
original review, Revision 1, its re-review and Revision 2. The publication manifest
records original and published checksums. This is a curated document backup,
not a complete raw evidence archive or authorization to run blocked experiments.
