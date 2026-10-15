# v2.6.1 release-blocker corrections

Implementation checks, **not independent release acceptance**. Another reviewer
must verify these corrections. Starting branch:
`hardening/v2.6.1-integrity-gate`, starting HEAD:
`0cd86d88a1cc2a57b6a13f8f0e8d003dadf85f59` (clean tracked source).

## Preserved evidence and unchanged-source reproduction

Read the independent report and adversarial results before changing kernel
source. The full independent audit was archived outside temporary/build storage;
see `audit-evidence/README.md` for SHA-256, inventory and readable report copies.

The unchanged kernel was fully rebuilt; its ELF SHA-256 matched the existing
artifact: `587709147dad6d91894d3cd2fc29cef0ef9bf9f0103e9e401579bf6781fdbf79`.
The pre-correction ELF is retained at `build/revise-blockers/before.elf`.
Original recovery image SHA-256 at takeover:

```text
4daf871c5c2982a2c6401a8a3336959f31bdd0143ae00f238f4cbdc7f453f19e
```

- Reran the seven failing QEMU cases from the original independent audit:
  **7 failed cases, 20 failing subtest assertions, no errors/skips**, in
  **77.317 seconds**. Malformed boot-240 entries replaced old snapshots;
  primary/mirror read and checksum failures resurrected PKG-201 ownership.
- Added repository regressions before fixes: **13 tests: 3 passed, 10 failed
  cases, 22 failing assertions, no errors/skips**, in **159.096 seconds**.
  Controls include valid field boundaries, a complete clean-boot chain and
  healthy 18-boot snapshot rotation.
- Evidence: `build/revise-blockers/before.json`,
  `build/revise-blockers/audit-reproduced-before/`, and
  `build/test-runs/revise-regressions-before/`.
- All tests use disposable images. The original recovery image hash was
  unchanged after both reproduction runs. Ring 3 remains held.

## Blocker 1: malformed journal evidence and snapshot progression

The single shared validator now enforces version/event/checksum plus 0/1
result and reversible flags, the supported risk enum, fixed action ID domain,
zero reserved byte, and printable, zero-padded six-byte tags. Package events
retain the complete package-byte range. Boot numbers retain their existing
8-bit representation. Occupancy remains separate: rejected entries are still
reserved and preserved byte-for-byte, but cannot affect ordering or consumers.

Snapshot writes require consecutive historical progression or domain-valid,
successful CLEAN_BOOT evidence for every intervening boot in a gap. A lone
high boot number remains diagnostic ordering evidence, not proof that a
16-slot ring aged enough to replace snapshots. Full historical preflight and
existing ambiguity/exhaustion locks remain in force. The check is bounded to
the existing journal and reads each containing sector once.

These are consistency checks, **not authentication**. A party able to rewrite
all mutually consistent metadata and checksums is not excluded by this format.
Uncorroborated gaps hold snapshot writes; healthy consecutive rotation and
complete clean-boot chains remain supported.

Focused build/test result: **32 passed, 0 failures/errors/skips, in 268.974
seconds**. Includes R5, F1, D2, N2, N3 and tagged downgrade/re-upgrade.
Evidence: `build/test-runs/revise-journal-fixed/summary.txt`.
No on-disk layout, checksum coverage, record size or version changed.
