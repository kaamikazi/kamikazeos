# First-tranche decision — design review only

## Recommendation

Do not treat **H0+A1+P1+R1** as one implementation-ready batch in the current
immutable design. The mathematical fault mechanisms are not rejected, but the
normative first-tranche predicates and observation contracts need revision.
This document makes no request for authority and grants none.

**Smallest defensible first consideration:** H0's bounded host-data-only core,
without guest hooks, VM launches, UART models or new kernel behavior. Reconsider
a corrected single A1 #UD parameter only after the design repairs below. P1 and
R1 should be separate subsequent decisions with their own precise preconditions.

References: D is `STAGE2B_DESIGN.md` SHA-256
`ae77ff66804a2ed98e8d2f0cab8b1ed15722749872c3ece782c7f72869a5c9fe`;
K is `${REPOSITORY}\kernel`; F01-F06 are the findings in the main review.
All proposed outcomes remain UNVERIFIED; nothing was built or tested here.

## 1. Should R1 require a prior successful R2?

**No, not as a hardware-execution dependency.** R1 is deliberately late: its
parent has delivered a real frame, switched to the fatal stack and published a
record before the secondary event. Actual pre-secondary PC/ESP/SS/TR/CR3 and
published record identity can prove those conditions for that instance.

Source proof: `K/src/emergency.c:108-110` captures and emits before setting
ordinary_authorized. The production code calls capture at 0x102e58, emitter at
0x102e6e and stores authorization at 0x102e8a. The current exact emitter entry
from that caller is a possible narrow semantic checkpoint; future offsets must
come from the new linked ELF. Require primary-pointer/caller qualification.

R2 instead injects before capture completion or on the boundary of ESP change.
It tests additional early partial-state/ownership behavior. Putting it first
would expand risk and would not replace R1's own runtime preflight. R1 does need
the **source-grounded R-N oracle and host controls** that distinguish old/current
stack, a secondary frame, counter/auxiliary capture and a genuine CPU exception.
It does not need an earlier R2 fault to be executed merely to believe the source.

## 2. Can A1 test low headroom without adjacent-page corruption?

**Yes under the declared baseline; not as an overflow/guard test.** For CPL0
32-bit gates, final hardware ESP is S-F. With S=B+F or B+F+1020, complete frames
occupy [B,B+F) or [B+1020,B+1020+F). F12/F16 remain inside a 16-KiB payload.
The page below B need not be touched, and flat SS does not fault at a linker
payload boundary. Common entry performs no old-stack software pushes before
moving ESP at 0x1000ea; its first C call is at 0x1000f2.

`K/src/emergency_frame.c:35-55` should decode those frames as valid.
`K/src/emergency_record.c:12-17` should reject their headroom. Therefore the
minimal corrected predicate is not "invalid frame": it is valid frame/origin,
the isolated BAD_STACK reason, eligible0 and the exact unsafe ordinary halt.

Before possible implementation authorization, F01/F02 must be closed:

- Replace the contradictory/ambiguous "valid/eligible classification fails"
  wording with explicit fields and distinguish record validity from frame validity.
- Require all other eligibility predicates true; do not let a locked store or
  wrong origin make the purported headroom test pass for the wrong reason.
- Define the strict post-pivot/pre-fault RAM baseline and partition source-frame,
  capture, bounded fatal-stack, fixture and CPU effects. Guard/table/other ownership
  regions stay immutable; normal record writes are not a corruption failure.
- Freeze one initial parameter, preferably #UD with S=B+F+1020, before exact
  zero-headroom and PF variants. Each variant is a separate case, not an implicit
  expansion after a first pass.
- Prove old-frame nonoverlap, correct maps/gates/SS/flags and no post-pivot spill.
  Any unexpected DF is failure, not a successful fallback rescue.

## 3. Can P1 distinguish unavailable storage from newly blocked writes?

**Not with zero ATA events alone.** A no-disk configuration can make lower writer
APIs return without a command. `K/src/recovery_store.c:483-486,577-584` supplies
that counterexample. A writer-call prohibition therefore needs static forbidden-
edge evidence and/or bounded entry-site observation, in addition to I/O sensors.
The current unsafe path is statically writer-free; that fact must be explicitly
bound to the new fixture/build, not inferred from an unavailable device.

P1 should be split and frozen:

1. Exact no-controller/no-disk configuration, actual initialization/availability
   proof and per-sensor applicability/calibration reference.
2. Exact existing locked-image seed recipe, valid checksums/generations, expected
   initialized state/journal lock/trust fields and full image before/after.

Architectural readiness must remain true. Only storage trust/availability is
varied. Use source-qualified fields or decoded loads, not bare duplicated local
symbol names. Snapshot after boot operations have settled and start the no-new-
request window at actual CPU fault onset.

The CMOS part is not optional. `K/src/recovery_memory.c:109-114` writes nine cells
before its conditional disk save. F04 requires a specified non-mutating state
export or a justified complete-trace reconstruction of the recovery cells. A disk
hash or a silent/unavailable IDE device does not satisfy CMOS equality.

## 4. Are H0 publication/checkpoint controls sufficient for R1?

They are the correct **topics**, not yet an established result or finite completed
catalogue. H0 was not implemented/run in the design, and none was run here.
Before considering R1 implementation, specify at least these control tuples:

- Published primary/frame-valid/origin-valid/eligible1, but ordinary_authorized0
  at the selected first emission point; this is a valid R1 parent state.
- Copied body with magic0 is not published, even if other fields look complete.
- Wrong record destination/caller, DF emitter instead of ordinary primary emitter,
  after-authorization point, changed parent byte and counter0/2 all reject R1.
- Correct reentry HLT PC without a genuine secondary vector/site/raw frame rejects.
- Direct unauthorized-formatter jump to the reentry halt rejects: source
  `panic.c:76,115` can take that shortcut without counter1/auxiliary capture.
- A debugger STOP/SIGTRAP is not guest #UD evidence. Require the real CPU event.
- Wrong task/SS/CR3/source/fatal stack, invalid frame extent, reset/shutdown,
  nonhalted correct PC, short/moving stability, and missing I/O evidence reject.
- Interrupted parent serial output is accepted only as the predetermined prefix;
  it does not make the healthy Stage 2A serial predicate optional.

Any UART backend/control work remains outside the narrow H0 core until its model
and observation protocol exist. No production state should be forged to make
one of these positive controls appear genuine.

## 5. Are all current first-tranche conditions deterministic and falsifiable?

Not yet as a batch. A1's hardware geometry is deterministic, but F01/F02 leave
its exact verdict/mutation contract ambiguous. P1 has F04/F05 observation and
causality gaps. R1 still permits multiple parent/checkpoint/admission variants
(F03). Different implementers could produce different experiments while claiming
the same case passed. Model confidence cannot close these gaps.

Numeric future instruction addresses cannot be known before a fixture is built;
that is not itself a flaw. The **semantic checkpoint, caller/destination, field
tuple, allowed effects and trace boundary** can and should be frozen first.
The later linked instruction/data-flow witness is an additional execution gate,
not a license to choose a different mechanism during implementation.

## 6. Recommended staged reconsideration, not authorization

| Gate | Evidence needed before the owner could consider the next separate proposal |
| --- | --- |
| H0 core only | Finite positive/negative data catalogue, immutable reference identities, no guest/device effects |
| One revised A1 #UD case | F01/F02 field/mask contract, isolated rejection cause and exact semantic pivot/fault boundaries |
| A1 additional positions/PF | Earlier case results independently reviewed; unchanged definition and new parameter explicitly scoped |
| P1 one named variant | F04 CMOS method, F05 no-call/I/O distinction, calibration and exact device/seed prerequisites |
| R1 one late checkpoint | F03 tuple and hook effects, H0 R-N controls, positive parent publication/stack proof and writer-free window |
| R2/R3 and other cases | Separate later case review; earlier risk ordering is not a blanket grant |

For any eventual run, retain the original deadlines, exact CPU/task/evidence
oracles, reset/triple-fault handling, image protection and fail/UNVERIFIED stop.
Preserve the initial failed Stage 2A case and do not repair it retrospectively.

This document does not authorize implementation, fault injection, builds or tests,
and does not ask for that authority on behalf of the owner.
