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
