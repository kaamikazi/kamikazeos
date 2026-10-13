# KamikazeOS v2.6.1 "Integrity Gate" (hardening, pending re-review)

v2.6.1 fixes the recovery, storage, package, manifest, and health-scoring
defects found by an independent read-only audit of v2.6.0. It adds **no
OS features**. Ring 3 stays held (`execution_approved=no`), nothing repairs
itself, and package and recovery policy is only tightened.

## Findings closed

| # | Finding (reproduced on v2.6.0 `f6c1814`) | Fix |
|---|---|---|
| 1 | A torn journal append was overwritten on the next boot. A damaged state block rewound the journal to slot 0 and overwrote history. | Append position rebuilt from the journal itself, never reusing an occupied slot. Generation + CRC-32 state with a mirror at LBA 50. Read-only store on ambiguity. |
| 2 | `rollback-pkg PKG-201` was accepted after PKG-202 updated the same module, undoing PKG-202. `apply-pkg` overwrote the only verified slot. | Ownership checks before rollback (`PKG-REJECT-001`). No apply over an unverified update (`PKG-REJECT-002`). |
| 3 | `seed-manifests confirm` blocked every built-in package and overwrote an imported PKG-101. | Disk seed copies trusted by exact identity with the compiled-in seeds, never labelled as signed. Seeding fills only empty slots, then package init re-runs. |
| 4 | A clean boot scored 55. Typos cost health and failed `verify-pkg`. Ring wraparound raised health from 30 to 80. An intentional `panic-test` cost 15 points on the next boot. | Health from classified production faults with per-boot counters. Clean boot is 80 (two simulated devices without drivers). |
| 5 | ACT-001's suggestion was journaled every boot. A full journal logged `STORE-WARN-004` five times in one boot. | Suggestions are journaled only on change. A full journal warns once per boot and counts refused appends. |
| 6 | No automated boot or failure tests. | Headless QEMU suite with failure injection on disposable images (`make test`). |

Details: `docs/STORAGE_INTEGRITY.md`, `docs/HEALTH_MODEL.md`,
`docs/TRUST_AND_SIGNATURES.md`, `tests/README.md`.

## Verification

```
python tests/run_regression.py --baseline-elf <v2.6.0 kernel>
```

- v2.6.0 `f6c1814`: **18 of 31** tests failed (the audit reproductions).
- v2.6.1 final: **33 of 33** passed in 193 s, including the downgrade
  test. The kernel builds with no warnings at `-Wall -Wextra`.
- Every test used its own disposable image under `build/test-runs/`.
  `build/recovery.img` was hash-checked before and after every run and is
  unchanged (`sha256 4daf871c...`).
- The analyzer and Recovery Center still read v2.6.0 serial logs. The
  dashboard renders a v2.6.1 report in a browser with no console errors.

## Storage compatibility

No migration. The LBA 1 bytes that v2.6.0 reads keep their meaning. The
extension lives in bytes v2.6.0 always wrote as zero, and the mirror uses
the previously unused LBA 50.

- v2.6.1 boots an image last written by v2.6.0: it upgrades in place on
  the first save (tested).
- v2.6.0 boots an image written by v2.6.1: it loads normally and the boot
  count continues (tested). v2.6.0 drops the extension and the
  intentional-panic flag when it saves, which degrades safely.

## Honesty boundary (unchanged)

- In-kernel Ed25519 verification is still **simulated**. The kernel checks
  host-written metadata. v2.6.1 adds a key-hash consistency check against
  the pinned anchor, which is not cryptography.
- Chained-update tests use fixture manifests carrying the host-verified
  marker. They are written only to disposable test images.
- Excluded health events are still logged, journaled where they were
  before, exported, and shown by the analyzer with a `health=` field.

## Still open (not in this hardening scope)

- The analyzer falls back to the whole multi-boot log if the newest report
  is truncated.
- No header dependency tracking in the Makefile (`build.ps1` always
  rebuilds everything).
- Paging: page 0 mapped, CR0.WP off, no guard pages. Memory above 16 MiB
  is tracked but not mapped.
- `boot_count` and journal boot numbers are 8-bit and wrap at 256.
- Heap rejections from `heap-alloc` still count as production warnings.
- The approved CPL3 round trip was not re-executed for this release,
  because Ring 3 execution stays disabled. The in-kernel privilege
  self-tests pass on every test boot.
