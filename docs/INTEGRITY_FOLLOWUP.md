# v2.6.1 Additional Integrity Corrections

Implementation evidence, not independent acceptance. Work started 2026-10-15
on `hardening/v2.6.1-integrity-gate`, with a clean worktree at
`7ccb4d9b32941d6f850a274c07e6ad0148bd51cb`.

## Fresh unchanged-source baseline

- Full rebuild using `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1`.
- Current ELF SHA-256: `1193c354337b96e342542d99803f7ad84600d4059b019a84953cef5a585da91a`.
- Compatibility source: freshly exported annotated tag
  `v2.6.0-pre-audit-baseline`, resolving to
  `f6c18148e6b003dfe48d3fdded3f83e603d95d2e`.
- All 225 exported source blobs matched tagged Git objects using Git's clean
  filter (`core.autocrlf=true`). The first raw-byte comparison stopped before
  building because the archive contained Windows line endings; canonical blob
  comparison resolved that verification-method issue.
- Freshly rebuilt compatibility ELF SHA-256:
  `ad05c568f49fa1e6cd685fd2e88dd64605cedb8ed33f7d60e51631fb78586a25`.
- All **53 existing tests passed**, no failures or skips, in **293.312 seconds**.
- Evidence: `build/integrity-20261015/provenance.json` and
  `build/integrity-20261015/regression/summary.txt`.

Original `build/recovery.img` SHA-256 before and after the baseline:

```text
4daf871c5c2982a2c6401a8a3336959f31bdd0143ae00f238f4cbdc7f453f19e
```

The original image is only read for hashing. QEMU uses disposable images.
Both current and tagged sources keep the Ring 3 approval macro at zero.

## Additional corrections

### N1: legacy-state misclassification

- Before: two tests produced five failing assertions (each of four damaged
  extension magic bytes, plus a nonzero reserved tail). The unchanged kernel
  chose `legacy_primary` over a valid mirror.
- Correction: require the complete zero reserved tail written by actual
  legacy kernels. Modern damaged state cannot bypass CRC validation.
- After: **16 tests passed**, no failures/skips, in **96.764 seconds**,
  including N1, F1, L1, all small disks and tagged downgrade/re-upgrade.
- Evidence: `build/test-runs/followup-n1-before` and `followup-n1-fixed`.
- No format change. Genuine legacy state and mirror generation floors remain
  compatible. Existing journal prefixes are preserved.
