# KamikazeOS regression tests

Headless QEMU tests for the v2.6.1 "Integrity Gate" hardening. Every test
boots the real kernel with `-display none`, drives the shell through the
QEMU monitor's `sendkey`, and reads evidence back from the serial log.

```
python tests/run_regression.py                  # build + run everything
python tests/run_regression.py --no-build -k F1 # one group
python tests/run_regression.py --baseline-elf old.elf   # + downgrade test
make test                                       # same, via make
.\scripts\run-tests.ps1                         # same, on Windows
```

## Safety rules the harness enforces

- Each test creates its own **disposable** 1 MiB disk image under
  `build/test-runs/<timestamp>/<test>/` and keeps the serial log,
  analyzer output and dashboard JSON next to it for review.
- `build/recovery.img` is never opened. The runner hashes it before and
  after the run and exits with code 3 if it changed.
- Failure injection (torn appends, bad checksums, both state copies
  damaged, equal-generation conflicting copies, full journals, corrupted
  TinyFS data) only edits those disposable images, between power-ons.
- No test approves or runs CPL3 code. The suite asserts that Ring 3 stays
  held (`execution_approved=no`) and that the in-kernel privilege
  self-tests (interrupt-frame layout, syscall origin policy, register
  preservation) still pass at every boot.

## Groups

| Group | What it proves |
|---|---|
| F1 journal integrity | torn appends, damaged state, corrupt entries, full and ambiguous journals never lose or overwrite evidence; legacy images upgrade in place; v2.6.0 boots v2.6.1 images |
| F2 package rollback | two sequential updates to one module; stale rollbacks and applies over unverified updates are refused |
| F3 manifest seeding | seeded built-ins stay usable, never overwrite imported manifests, and are not labelled as signed |
| F4 health score | typos, canaries and intentional tests do not cost health; real faults do; ring wraparound cannot raise the score |
| F5 journal longevity | unchanged suggestions are not re-journaled; a full journal costs health once |
| R1 regression | Ring 3 hold, TinyFS, recovery actions, canaries, host manifest tooling, analyzer and dashboard compatibility |

## Test fixtures and trust

Chained same-module package tests need signed manifests that no real
example provides. `kmz_disk.fixture_manifest_sector()` writes the
host-verified marker byte directly into the disposable image. That marker
is exactly what the kernel's **simulated** signature check reads, so the
fixture stands in for "a manifest the host tooling verified". It is not a
signature and is never written to `build/recovery.img`.
