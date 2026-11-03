# v2.6.1 reset-boundary correction and complete verification

Implementation and verification evidence, **not independent release acceptance**.
The combined slot-transition and reset-boundary correction is uncommitted beyond
`9d5f43e42e667390ad5fc2b69953b79e9753e5cd` on
`hardening/v2.6.1-integrity-gate`. No new commit is implied by the hashes below.

Related documents: [required invariants](TRUST_INVARIANTS.md),
[slot-transition history](SLOT_TRANSITION_CORRECTION.md),
[architectural correction history](TRUST_CORRECTIONS.md), and
[regression harness](../tests/README.md).

## Scope and findings

An independent read-only review verified the earlier 135-case suite and ordinary
slot-transition correction, but recommended REVISE for two pre-existing reset
exception gaps. The review's additional boundary checks passed 30/33 on that
snapshot; the three failures concerned duplicate factory identities, non-factory
version-array tail bytes, and reusable grants after rejection. They were writer
boundary findings, not established shell/CPL3 exploits.

The prior source and review evidence are preserved locally under
`build/preservation/20261017-pre-F1F2/`, including `files/`, `working-tree.diff`,
`working-tree.sha256`, and the copied `review/` report, fixtures and results.
The current changes address F1/F2 without redesigning recovery or changing the
persistent formats.

## F1: exact canonical factory-image validation

Previously the reset exception checked each proposed record independently with
`module_slot_is_pristine()`. A record could be pristine for its proposed module
yet occur at the wrong position or duplicate another module. String comparisons
also ignored bytes after NUL, although the serializer writes those bytes. The
reset early return bypassed the ordinary candidate/transition checks.

The correction uses one canonical constructor:
[`module_slot_factory_record()`](../kernel/src/module_slots.c), also used by
`load_defaults()`. It zeroes the whole record before populating the fixed module
identity, base versions and flags. Its read-only declaration is an additive
kernel C API in [module_slots.h](../kernel/src/module_slots.h); it grants no write
permission.

`exact_factory_image()` in [package_authority.c](../kernel/src/package_authority.c)
compares every proposed record against the canonical record at the same index.
`same_slot_record()` compares all serialized fields and both complete eight-byte
version arrays, excluding C struct padding. Therefore reset requires:

- The fixed identity order: GhostAttach, RecoveryEngine, AI Rules, RC Schema.
- Their factory versions: `0.3`, `0.5`, `0.4`, `0.8`, respectively, in slot A.
- Slot B equal to `-`, with unused array bytes zero in both version fields.
- Slot A active and verified; slot B unverified; no rollback pointer or pending
  verification.
- A legitimate reset grant and already acknowledged empty durable package state.

Duplicate/reordered identities, foreign identities, changed flags and nonzero
array-tail bytes are rejected before the slot-sector ATA write. The existing
serializer zeroes reserved sector bytes and computes the existing checksum.
Ordinary updates cannot use factory appearance as a reset exemption.

## F2: one-shot reset-grant authorization

The RAM-only grant still requires jointly trusted, non-staged durable evidence.
The normal workflow remains:

```text
begin_reset -> acknowledged empty package state -> canonical factory slots -> end_reset
```

The grant spans the authorized state-clear step; it is not consumed by a
successful empty-state save alone. It is consumed by the final slot authorization
attempt, whether that proposal is accepted or refused. Specifically:

- A nonempty state proposal, out-of-order slot proposal, invalid factory image,
  latched uncertainty or untrusted package copies revokes the held grant.
- The authorized factory slot attempt consumes it before ATA I/O, so a successful
  reset cannot reuse it and a failed slot write cannot retain it.
- Persistence failures reported by the state/slot writers retain their existing
  uncertainty holds. A reported failure of both package copies or of the slot
  write revokes the grant through the uncertainty latch.
- A corrected retry requires a newly acquired grant; acquiring it is possible
  only if the remaining durable evidence still satisfies joint trust.

Both [module_slots_save()](../kernel/src/module_slots.c) and
[package_state_save()](../kernel/src/package_state.c) consult authority before
their other early-return guards, so those guards cannot skip the grant's
authorization decision. The normal reset wrapper still ends the grant on return.
Partial reset is not resumed automatically after reboot: contradictory evidence
holds rather than manufacturing completion.

## Six earlier slot-transition failures remain corrected

The combined proposal retains `slot_edges_valid()`: every ordinarily changed
module is checked against its acknowledged durable predecessor and its own
matching, bound, durable stage. Only the specific forward APPLY/VERIFY/ROLLBACK
changes are permitted; unaffected serialized fields remain identical. Any
unauthorized module rejects the whole sector proposal before ATA writes.
Recovery classification is separate: reconciliation publishes the observed
package result without rewriting slots to create evidence for it.

The earlier failing regressions, retained unchanged, cover:

1. Reversal of an acknowledged APPLY while its stage remains outstanding.
2. Reversal of acknowledged VERIFY under VERIFY_STAGED.
3. Reversal of acknowledged ROLLBACK under ROLLBACK_STAGED.
4. A stage for one module authorizing changes to another.
5. A valid target-module change mixed with an unauthorized other-module change.
6. An unrelated inactive verification-flag change during rollback.

## Before/after reproduction evidence

Paths below are under `build/test-runs/`. Repeated runs overlap and must not be
added as independent case coverage. Historical evidence is retained unchanged.

| Run | Passed | Failed | Errors | Skips | Seconds |
|---|---:|---:|---:|---:|---:|
| `slot-transition-before` (unchanged HEAD kernel) | 14 | 6 | 0 | 0 | 4.043 |
| `slot-transition-fixed-actual` | 20 | 0 | 0 | 0 | 3.356 |
| `slot-transition-full` (pre-F1/F2 snapshot) | 135 | 0 | 0 | 0 | 991.601 |
| `F1F2-before` (reset regressions before F1/F2 behavior changes) | 5 | 8 | 0 | 0 | 1.508 |
| `F1F2-after` | 13 | 0 | 0 | 0 | 1.580 |
| `F1F2-focused` (ownership/latch/reconciliation, slot and reset controls) | 40 | 0 | 0 | 0 | 106.555 |
| `full-148-reset-20261017-080017-edc09f` | **148** | **0** | **0** | **0** | **1008.857** |

The eight failing F1/F2 regressions were duplicate identities, reordered
identities, inactive/active array tails, rejected slot/state proposals retaining
the grant, out-of-order attempts retaining it, and successful reset retaining it.
The other five were positive or previously enforced negative controls. All
thirteen cases are registered in [test_reset_boundary.py](../tests/test_reset_boundary.py).
The [actual-C fixture](../tests/reset_boundary_fixture.c) constructs expected
factory records and sector bytes independently of the kernel's canonical builder
and checks rejected candidates leave disk bytes and write counts unchanged.

The earlier `F1F2-full/summary.txt` is incomplete and has no terminal run summary;
it is not counted as successful complete verification. The fresh completed run
below supersedes it. The earlier slot-only intermediate fixture error remains
documented in the slot-transition history rather than being counted as a pass.

## Completed 148-test verification

Evidence directory:
`build/test-runs/full-148-reset-20261017-080017-edc09f/`.

The complete `summary.txt` contains all 148 expected case IDs and ends:

```text
Ran 148 tests in 1008.857s

OK
```

`result.json` records actual runner exit code **0**, no incomplete/unexpected
cases, and all 13 reset cases as `ok`. Test time is **1008.857 seconds**; runner
wall time including the fresh build is **1017.051 seconds**; total supervision
and preservation-check time is **1020.112 seconds**. Windows accepted detached
process/job breakaway; `launch.json`, `running.json`, `console.log` and
`supervisor.log` retain the launch/build/termination evidence.

The 148 cases comprise the prior 135 plus 13 reset-boundary cases. They retain
forward A/B lifecycle, staged retries and observed before/after reconciliation;
uncertainty/latch, partial-reset and degraded-current controls; ordinary slot
authorization; and compatibility, TinyFS, host-consumer and privilege-hold checks.
The missing-gap journal control preserves snapshots across real later boots
19/20/21, while the matched canonical-gap and 18-boot rotation controls pass.

Exact runner command (fresh build, no test filter):

```powershell
python -B tests/run_regression.py --baseline-elf "C:\Users\imran\AppData\Local\Temp\opencode\kmz-slot-review-9d5f43e\baseline\build\kamikaze.elf" --run-dir "build/test-runs/full-148-reset-20261017-080017-edc09f"
```

For a new reproduction, choose a fresh run directory. The compatibility baseline
was independently exported/built from
`f6c18148e6b003dfe48d3fdded3f83e603d95d2e`; its local ELF SHA-256 is
`353af7766e769a5e21be95ffd0708b487d6edbaca9291f6ac1fb06921103e032`.
LLVM, Node.js, QEMU and Python are required. The local absolute baseline path is
an evidence location, not a portable dependency bundled with the proposed commit.

Verified run-artifact SHA-256 receipts:

| Artifact | SHA-256 |
|---|---|
| `summary.txt` | `a4f784d8dcb7e8e8c454204aff8a96d26e07964a02202ad49d390803b85ffdae` |
| `result.json` | `123f817e8444627789e02c0320d9d9179c23b203e9ab4297c9e1af3497caccdd` |
| `preflight.json` | `1b13186480cd47968dd758e81757366a25dfe2deb4735ed3cebc77768c7dc601` |
| `source-before.json` | `61d2a6864f9d75c710429e17c669d7b2170f1fab8c2c6804e20207161a91f108` |
| `launch.json` | `72894db8724be2e23d45719081059919b703e25a2e477e4a4d964ca2ab33720d` |
| Tested `build/kamikaze.elf` | `c1c9458e3cc6e7491decef8aef624e553252c55efed2d35d5b766a358aad1c78` |

## Tested executable-source and fixture receipts

All changed executable inputs below match the fully tested snapshot byte for
byte. The documentation preparation changes only Markdown. `source-before.json`
inventories all 285 tested working-tree files; the ten changed executable inputs
are listed here explicitly. These are raw working-file SHA-256 values, distinct
from Git's normalized blob IDs.

| File | SHA-256 |
|---|---|
| `kernel/src/module_slots.c` | `025771d173974d11ae00e35794f156d678075f1b939b5106a866d1830fd1c0e0` |
| `kernel/src/module_slots.h` | `ce99cca73977536cb21414417785d96421e33656de92cd2d8c5f86f7bc704d14` |
| `kernel/src/package_authority.c` | `d17f4902e2f85264182920f9ae02eac460612636328e123da1dd815a8d359679` |
| `kernel/src/package_state.c` | `f95f17929509a1d15b8992bd1d62a4cad1938b4ec180f30bacd6d835f3e08778` |
| `tests/reset_boundary_fixture.c` | `032b40b9697727839bc904afe8b1ca3a2a87a9541e981fc4ab9db991268d05b3` |
| `tests/slot_transition_fixture.c` | `cf96cb0ce7123d6035526c9c68fa5c128426b52e7b8b75e658bbeb726aab7bbc` |
| `tests/test_reset_boundary.py` | `56c86f556b9a721a8f33bd12db819e2bb1e854480715bd4e4189d3cb94aec7e3` |
| `tests/test_slot_lifecycle.py` | `a56bfb6dbdaea2c2dc1b89c127fc1f83661691af0d020292b7dad7643b51567d` |
| `tests/test_slot_transitions.py` | `1a813f89e88091f9ed059f4d29859821d7b994c68436ba77a15bcdf6fc917810` |
| `tests/test_trust_journal.py` | `656f9f6f6c8fca006fa768b7ec2f112739f643b65d33e116f1f29cde1b775d30` |

The exact proposed commit comprises these ten inputs and five Markdown files:
this document, `docs/SLOT_TRANSITION_CORRECTION.md`, `docs/TRUST_CORRECTIONS.md`,
`docs/TRUST_INVARIANTS.md`, and `tests/README.md`. The final local review package
under `build/commit-review/reset-boundary-9d5f43e/` inventories all fifteen files
with their final hashes and provides a reproducible proposed patch/tree receipt.
Generated evidence, images and historical audit files are not in that scope.

## Compatibility, preservation and remaining risks

- Disk magic/version, record sizes, field offsets, checksum coverage and LBA
  ownership are unchanged. Reset still writes the existing package sectors
  48/49 followed by the existing slot sector 56. No migration or persistent
  reset/transaction identity was introduced. Existing public functions retain
  their signatures; the canonical-record helper is the only additive C API.
- Previously unauthorized proposals now fail. The successful baseline
  downgrade/re-upgrade tests establish exercised compatibility, not safe
  enforcement by old kernels with weaker policies.
- Original `build/recovery.img` remains
  `4daf871c5c2982a2c6401a8a3336959f31bdd0143ae00f238f4cbdc7f453f19e`.
  Historical audit archives remain
  `f5982850ab1d46a4ca4f0d9bdb09fab31311abc9e66f64387848ae8a10235365`
  (`release-8e75093.zip`) and
  `bae909075761af2c3edafe33e993002487d79cc61c9c0dae483ffccb2b75be75`
  (`release-0cd86d8.zip`). All historical audit files and run receipts are retained.
- `RING3_ROUNDTRIP_TEST_APPROVED` remains `0`. This work includes no CPL3
  execution, privilege-source changes, merge, push or v2.6.2 work.
- Atomic rejection means no ATA write before authorization; it does not prove
  physical sector/flush atomicity. Reset spans multiple sectors and is not
  crash-atomic. An interrupted inconsistent reset can require separately
  reviewed intervention rather than automatic repair.
- Package updates remain metadata-only. Checksums and joint consistency do not
  authenticate hostile coherent rewrites or provide authenticated anti-rollback.
  Kernel signature verification remains simulated. No concurrent external-writer
  or arbitrary-kernel-memory-corruption guarantee is added.
- Actual-C fixtures control ATA/manifest/health inputs and model cold BSS.
  Complementary QEMU lifecycle/failure checks exercise the real kernel, but do
  not replace physical power-loss testing or independent acceptance review.

This is a local commit proposal. No commit has been created. Explicit
authorization is required before committing; passing regressions and a prepared
tree do not constitute independent release acceptance.
