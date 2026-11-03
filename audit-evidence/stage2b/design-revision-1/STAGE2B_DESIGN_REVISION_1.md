# KamikazeOS v2.6.2 — Stage 2B Design Revision 1

## 1. Status, authority and document set

**DESIGN ONLY — all new Stage 2B outcomes are UNVERIFIED / NOT RUN.** This is
a response to the independent verdict **REVISE BEFORE AUTHORIZATION**. It is
submitted for independent review; it is neither acceptance nor implementation or
execution authority. No build, test/control, VM, injection or fault fixture was
performed or created in this revision.

Production is `${REPOSITORY}`, branch
`feature/v2.6.2-memory-protection-phase-b`, HEAD
`dbf23ce74a2a7103ae6fce04fba51969b7807178`, tree
`62ff1f4fbd5d70163c074fb7c259f4f2115c4d77`.

The immutable original design is
`${LOCAL_EVIDENCE}\kmz-v262-stage2b-design-dbf23ce\STAGE2B_DESIGN.md`,
SHA-256 `ae77ff66804a2ed98e8d2f0cab8b1ed15722749872c3ece782c7f72869a5c9fe`.
All six deliverables in sibling directory
`kmz-v262-stage2b-adversarial-review-dbf23ce` are authoritative review inputs.
Their hashes and read inventory are in `STAGE2B_REVISION_PROVENANCE.json`.
The original documents remain unchanged. Within this **new proposal**, the
explicit contracts below replace the ambiguous predicates identified by F01–F06.
An ambiguity between revision documents blocks authorization; it is not resolved
by choosing the less restrictive reading.

| Revision document | Responsibility |
| --- | --- |
| This document | Scope, named mechanisms, exact semantic choices and open prerequisites |
| `STAGE2B_REVIEW_FINDING_RESOLUTIONS.md` | F01–F06 dispositions and residual evidence gaps |
| `STAGE2B_AUTHORIZATION_LADDER.md` | Separate review, implementation, calibration and execution decisions |
| `STAGE2B_MUTATION_BOUNDARIES.md` | Baseline epochs, RAM ownership, bounded deltas and persistent state |
| `STAGE2B_ORACLE_MATRIX.md` | PASS/FAIL/UNVERIFIED rules, H0 catalogue, CMOS and writer observation chains |
| `STAGE2B_REVISION_PROVENANCE.json` | Input/output identities, inspection provenance and zero-execution record |
| `STAGE2B_REVISION_PRESERVATION.md` | Before/after protected-content comparison and its limits |

## 2. Evidence boundary and inherited baseline

Notation shared by the revision documents: sizes, counts, offsets, LBAs and
exception-vector numbers are decimal. Architectural shorthand CS08/SS/data10,
TR28/TR30 denotes selectors `0x08/0x10/0x28/0x30`; CMOS60..68 and ports70/71 are
`0x60..0x68` and `0x70/0x71`. Raw status/command/CMOS byte strings are hexadecimal;
other command and unsafe-mask values use an explicit `0x` prefix.

Here **K** means production `kernel/`; **P** means the inspected production ELF,
SHA-256 `7cc1262fb1caee2b104d17d601a19e172eb02eac6d93b56ad2b2612a228db7bc`.
`PRODUCTION_DISASSEMBLY.txt` and `PRODUCTION_REFERENCE_BINDINGS.json` are fresh
static receipts about P. A number marked P is not a future fixture offset.

Historical acceptance is retained in its original scope:

- Stage 1 ordinary faults/intentional panic: 202/202 historical regressions.
- Independent Stage 2A: the fixed genuine #PF-during-#PF task-gate experiment,
  two accepted instances and 66/66 compatibility cases. Its accepted ELF is
  `79415990686f9ab0579c6f86f216e1fd7d372965eb7e7bf2f0e7e952ffecdd50`.
- Keep its original 62 assertions plus four guards, healthy serial requirements,
  exact-ELF inline-aware halt derivation, 60-second boot / 30-second post-confirm
  deadlines, reset detection and storage checks intact. New profiles get new IDs.
- The initial Stage 2A observer failure remains `passed=false`, exit 1, SHA-256
  `70600df8e31af2bfc35235bc03310c913160fd58c5ef59cc39ef68abcca19a1a`.
  The optimized inlined halt explained a later observer correction; it did not
  retroactively turn the original result into a pass.

This revision inspected source, bytes and preserved data. It establishes no new
runtime outcome, physical-hardware result, general stack-overflow guarantee,
UART-failure result or independent acceptance.

### 2.1 Non-negotiable future execution conditions

Every later proposal must separately bind a fresh build, source/ELF/tool hashes,
observer, emulator/CPU/firmware/model hashes, full launch arguments and one CPU.
Independently decode the actual linked control flow, local labels, injection
instruction and all allowed terminal loops. Live code patching, debugger-generated
signals, INT3 and direct handler jumps cannot substitute for a real CPU exception.

Preflight actual descriptors, TR, CR0/CR3/CR4, mappings, source and diagnostic
payloads and metadata accessibility. Current baseline: CPL0, flat 32-bit SS10,
CS08/data10, TR28, CR3=`0x00148000`, CR4=0, WP clear, five USER leaves, ten mapped
inactive reservations and mapped NULL; Ring 3 execution remains held. Do not
enable protections or modify paging/task metadata to make a case work.

Initial crash records, entry state, latch and reentry counter must be zero in a
fresh instance. Readiness must be established from qualified RAM/decoded loads,
not debugger calls into guest C. Healthy-UART cases retain serial readiness plus
actual settled-boot/I/O state. Arm/ack is a separate one-shot test-build contract.

Use the accepted CPU/I/O trace routing, immutable window offsets and positive
sensor controls. Keep the 32-MiB trace cap, bounded debugger/QMP requests, monotonic
deadlines and terminal observations at least 250 ms apart. Exact PC alone is not
halt evidence: require HLT=1, IF0, correct task/selectors/root/ESP and no continuation.
Any reset, triple fault, unexpected vector/task, wrong state, writer, timeout or
lost observation stops the sequence and preserves the complete failed instance.

## 3. Exact first low-headroom candidate: A1-UD-1020

This is **one** #UD experiment, not the original UD/PF × position family.

### 3.1 Mechanism and preconditions

Let B/T be the future linked bootstrap payload bounds; P has
`[0x0014e000,0x00152000)`. Require the identical 16-KiB classification and a flat
32-bit same-CPL interrupt gate (present DPL0 type/access `0x8e`) targeting ISR6.
The live caller ESP is at least B+4096 and its actual active-frame extent is
disjoint from `[B,B+1032)`. Preserve that live-stack snapshot separately.

All persistence predicates except headroom must be independently true:
runtime ready, valid DF/task foundation, trusted available RecoveryStore,
state_locked=journal_locked=0, journal_scan_valid=journal_order_valid=1,
boot_order_trusted=1, ATA depth=0, correct text EIP/root/CS/context and no guard or
paging-structure fault. A locked store cannot mask an A1 failure.

The future default-off fixture consumes its arm/ack on the healthy original
stack, disables maskable interrupts and pivots ESP to **S=B+1032**. Its very next
instruction is its one named UD2, with no intervening call, push, spill, memory
access or second arm action. The UD2 label and numeric addresses are **U-BUILD**.
P's existing `interrupts_trigger_invalid_opcode_test` has a prologue at
`0x00107c30..32` and UD2 at `0x00107c33`; calling it after a pivot would change
the geometry and is not this case.

Freeze the strict source/RAM baseline **after pivot, immediately before UD2**, IF0.
For F=12 the hardware frame is `[S-12,S)`:

| Item | Required value; P illustration |
| --- | --- |
| Pre-fault ESP S | B+1032 = `0x0014e408` |
| CPU frame ESP | B+1020 = `0x0014e3fc` |
| Saved EIP / CS / EFLAGS DWORDs | At CPU ESP +0/+4/+8; EIP is the linked UD2, CS08, flags match independently decoded fault delivery |
| Origin ESP | S, reconstructed from the complete 12-byte same-CPL frame |
| Source headroom | 1020, strictly below 1024 |

The complete frame remains inside the payload. No inactive reservation delta or
software table write is permitted; only enumerated hardware A/D effects may
change table bits under M5. Synthetic PUSHA-era ESP in entry state is CPU ESP−8; it is
not a software push, a followed pointer or the origin ESP.

### 3.2 Required result

Exactly one genuine vector6 at the selected UD2, CPU trace i=0, no vector8/reentry;
profile O on the fatal diagnostic stack. Require a magic-published primary with:

`version=1, kind=1, vector=6, error_code=0, cs=8, context_source=1,
frame_valid=1, origin_valid=1, eflags_known=1, cr2_applicable=0,
stack_kind=1, cpu_esp=B+1020, origin_esp=B+1032,
unsafe_flags=0x04, persistence_eligible=0`.

CR0/CR3, raw flags, GPR entry bytes, source bounds and EIP must agree with actual
CPU/raw-frame evidence; #UD does not supply a new CR2 address. The other unsafe
bits must all be clear. Publication, valid frame, valid origin and eligibility are
four separate properties. Require `ordinary_authorized=0`, latch1/counter0,
unchanged zero DF/auxiliary records and no ordinary dispatcher/formatter/writer.

The P unsafe terminal post-HLT is `0x00102ec2`; dispatcher-return fallback
`0x00102eb2` fails this profile. The future exact ELF must rederive this distinction.
Healthy output includes the complete primary numeric block. Persistent state is
equal and no forbidden writer edge/request occurs. RAM deltas obey the separate
mutation partition, not a global prohibition on required capture/stack writes.

An invalid frame/origin, extra unsafe bit, eligibility1, DF rescue, extra source
write, normal formatter, writer or wrong halt is FAIL. Missing nonoverlap, exact
binding, bounded-stack proof or qualified CMOS/writer coverage is UNVERIFIED and
blocks execution. The single candidate is pinned for independent **design** review;
U-BUILD/U-RAM/U-CMOS/U-WRITER remain open execution prerequisites.

The B+12 zero-headroom #UD and both F16 #PF positions are separate deferred
parameters. This candidate tests policy headroom, not recursion or guard activation.

## 4. Exact late reentry candidate: R1-UD-EMIT0

### 4.1 One parent and one checkpoint

Parent: the existing ordinary #UD trigger
`interrupts_trigger_invalid_opcode_test`, its UD2 P:`0x00107c33`, on a healthy
bootstrap stack with >=1024 bytes of headroom. No source pivot. The future build
must identify that same site's bytes/data flow. Require runtime/storage ready,
ATA idle, TR28 and all admission predicates true. The primary must be published,
kind1/vector6, valid frame/origin, `unsafe_flags=0`, **eligible1**.

Checkpoint **C-EMIT0** is first primary-emitter entry from
`emergency_handle_entry`, before the emitter's original first instruction/prologue,
after successful primary publication and before the ordinary-authorization store.
P calls capture at `0x00102e58`, emitter at `0x00102e6e`, returns at
`0x00102e73` and stores authorization at `0x00102e8a`. P emitter entry is
`0x001037e0`. Qualify all of:

- Caller/return address is that ordinary call site, not DF or a generic sink hit.
- Cdecl stack arguments identify primary, `console_put` (P:`0x00102f20`) and
  unused context0; in P `[ESP]=0x00102e73`, `[ESP+4]=0x0013dbc0`,
  `[ESP+8]=0x00102f20`, `[ESP+12]=0` at entry.
- All 128 primary bytes and 100 decoded entry-state bytes are captured.
- Latch1/counter0, auxiliary magic0, `ordinary_authorized=0`, current ESP and the
  complete prospective `[ESP-12,ESP)` are inside the roomy fatal payload.
- Real store readiness1/depth0 persists at this point. No writer has run since
  the parent fault. A primary eligible1 does not imply current authorization1.

The future one-shot secondary hook is **C-SEC**: one UD2 immediately before the
emitter's original first instruction, after only its declared arm-consumption/ack
prefix. That prefix may touch only the two fixture DWORDs; no call, push, spill,
GPR/ESP clobber or primary/entry/authorization store. Its effect on arithmetic
flags must be decoded and captured, not mistaken for the parent's saved flags.
No primary-emitter byte has been sent at this checkpoint. The exact secondary
instruction and fixture addresses do not exist in P: **U-BUILD**, not an invented
numeric address. Stop and snapshot immediately before this real UD2.

### 4.2 Required result

Exactly [6,6], both genuine CPU faults at their two named sites, no DF. Profile
R-N/TR28, secondary frame12 on the current fatal stack, terminal
ESP=`ESP_C-SEC−12`. Auxiliary is published kind2/vector6, counter delta exactly1;
parent128 and entry100 remain byte-identical. Ordinary authorization remains0;
no C recapture, parent output, normal formatter, writer or abandoned-path resume.
Healthy UART output for this window is exactly the fixed reentry literal
`[EMERGENCY_REENTRY]\r\npersistence=skipped\r\n`.

The auxiliary record is deliberately partial: it supplies vector and observed
control registers, not a newly decoded EIP/stack/GPR frame. Read the separate raw
secondary frame. Its kind2/unsafe_flags=0x40/eligible0 must not overwrite the eligible1
parent. P reentry terminal post-HLT is `0x001001c7`; bind the new ELF afresh.

An already-ineligible parent, wrong destination/caller, post-authorization hook,
later output byte, debugger-only stop, unauthorized-formatter direct jump to the
same halt, counter0/2, changed parent or recursive C path cannot pass this ID.
**Prior R2 execution is not required.** R1's own published-parent/current-fatal-
stack observations and independently reviewed R-N controls establish its instance.

## 5. P1 is three storage profiles

All use one ordinary #UD on a roomy bootstrap stack, runtime/DF readiness1,
depth0, valid frame/origin and only storage admission denied. Required primary:
`unsafe_flags=0x10` (BAD_STORAGE only), eligible0, profile O. Actual topology/trust
must be read independently; warning text or forged flags cannot establish it.
Writer-entry absence **and** qualified ATA/CMOS evidence are mandatory.

| ID | Frozen cause and required initialized state |
| --- | --- |
| P1-NC-UD | No primary ATA controller decoded at 1f0..1f7/3f6; IDENTIFY path's first status is ff, returns before command ec. ATA present0/capacity0/depth0; store valid0, locks0, scans/order0, source none, generation0, boot_order_trusted0. Exact emulator topology/launch proof U-DEVICE. |
| P1-ND-UD | Primary controller exists, no primary-master disk; real ec IDENTIFY then status00 branch. Same initialized unavailable-store tuple as NC. No-primary-master alone is insufficient if another route changed the driver outcome. Exact model/configuration U-DEVICE. |
| P1-LK-UD | Real present 2048-sector disk, ambiguity recipe below. valid0; state_locked=journal_locked=1; scan=order=1; source ambiguous; mirror present1; in-RAM generation0; jcount3; boot_order_trusted0. No lock or depth forgery. |

### 5.1 One locked-image recipe, no image created here

Starting reference is the preserved Stage 1 pre-trigger image recorded in
`READ_ONLY_SEED_REFERENCE.json`, SHA-256
`2d1a74227f1ec230a60eb447d534ed5bd67866ee859ca377f64c549c3b3413d6`,
1048576 bytes. Original primary LBA1 and mirror LBA50 are identical, boot1,
generation5, jcount3. `SEED_JOURNAL_INSPECTION.json` records their valid checksums/
CRCs and three journal entries, all boot1 with one CLEAN_BOOT.

Only if separately authorized later: use a **new isolated copy**, leave primary
and every other sector intact, set mirror byte5 to200, recompute byte17 as the
two's-complement sum over bytes0..16, and recompute IEEE CRC32 of bytes0..47 into
bytes48..51 LE. Leave mirror generation5 and all remaining bytes unchanged. This
is the existing `tests/test_integrity_gate.py:158–179` ambiguity pattern using
`tests/kmz_disk.py:226–237` resealing rules; those tests/functions were not run.
The resulting copy/hash is **UNRESOLVED — NOT CREATED**.

Both copies would be internally valid and same-generation but unequal.
`K/src/recovery_store.c:272–280,330–383` then selects ambiguity before loading a
trusted copy. Thus its **in-RAM generation is 0**, not the on-disk 5. Journal scan
is complete/ordered, append position3, lower bound/max boot1; journal locks hold
boot appends without advancing jcount. Boot refusal counters may increase before
the strict baseline; freeze their observed values there, require no later change.

Require a fresh VM's initially invalid CMOS load, established by its original
guest-read trace, not a host write. `recovery_memory_init:174–202` then derives
backend `disk+CMOS`, untrusted boot order and boot_count2 from the journal lower
bound. It still writes CMOS during normal boot; clean-boot save also precedes the
fault window. State/mirror/journal must remain recipe-identical through boot;
other boot activity must be separately accounted for before taking the settled
whole-image baseline. No primary repair or "make-valid" command is allowed.

P1-LK-UD is the first storage candidate for later consideration. NC/ND are separate
deferred topology proposals. Disk equality for a truly unattached disk is **N/A**,
not a media-denial pass. All three still require CMOS equality and no writer entry
even when an API would return before issuing any ATA I/O. See oracle sections
W/CMOS and the per-profile calibration rules.

## 6. Precisely split later checkpoints

These definitions close range ambiguity; they do not bring these cases into the
first tranche. Numeric entries below are P references only. Every hook/binding
still has U-BUILD and the common observation gates.

| ID / selected boundary | Pre-secondary/first-fault state and required result |
| --- | --- |
| R2-PRE-ESP-UD: immediately before common MOV ESP; P `0x001000ea` | Parent UD delivered, latch1; fixed assembly stores and selector reloads complete. Current ESP=parent hardware ESP on boot stack. No C decode/capture: primary128 zero; entry100 preserves this partial state. One secondary UD writes12 bytes immediately below parent frame; R-N terminal uses that boot-stack ESP−12. |
| R2-POST-ESP-UD: after alignment, before first C CALL; P `0x001000f2` | Same partial entry bytes, primary zero; current ESP=fatal top, before CALL pushes anything. One secondary UD writes `[fatal_top−12,fatal_top)`; R-N terminal ESP=fatal_top−12. No C capture resumes. |
| R3-PRE-MAGIC-UD: immediately before primary publication store; P `0x001037be` | The 128-byte memcpy has returned and stack cleanup at P `0x001037bb` completed; ESI points to primary; magic0 and completed copied body captured. One secondary UD; parent128/entry100 byte-identical, magic remains0. Byte preservation can pass; primary semantic record validity remains UNVERIFIED. |
| P2-READ-CMD-UD: immediately after command OUT; P `0x00101dab` after OUT `0x00101daa` | One owned first-attempt READ0x20 submitted to primary master, sector count1, frozen LBA/buffer, depth1, latch0. No post-command status poll or INW/data copy yet. One primary UD leads O. Because storage_ready rejects active I/O, require **unsafe_flags=0x30 (BAD_STORAGE + IO_ACTIVE)**, not0x20 alone; valid frame/origin, eligible0. |

R2's pre/post partial entry has vector6/has_error0/kind1, observed original GPRs/
segments/CRs and CPU ESP. Its saved frame fields at offsets52..72 are still zero
and frame.esp is still the hardware ESP; the decoder has not established the
later PUSHA convention. Do not promote the incomplete primary/entry bytes.

P2's candidate read is one raw sentinel LBA2047 on a separately owned 2048-sector
copy, with a dedicated 512-byte fixture buffer. Future static layout review must
prove nonownership by every current format and bind the buffer; neither is a
present fixture. Pin command/LBA/sector count, zero transferred words, no retry,
actual depth and an independently observable model phase/status at the boundary.
The first *guest* post-command status has not been sampled. Backend read completion
may already have happened: no outstanding-host-read claim is permitted. U-P2
includes the unprovided model/status observation and exact linked hook witness.

P2-READ-DRQ0-UD (first INW, P `0x00101de0`, zero words transferred after DRQ check)
is a different deferred data-ready variant. Mid-transfer, wrapper-return-only and
backend-pending variants are also different deferred designs. No positive depth
alone selects among them. No ATA command/data write/flush/CMOS or writer entry
may follow fault onset; only completion of the preowned read is admissible. For
the selected pre-transfer case the buffer is unchanged after fault and image
equality is absolute. P3 WRITE/FLUSH remains deferred pending an exact ledger.

## 7. Ownership, oracles and unresolved prerequisites

The six RAM classes and epoch-specific rules in
`STAGE2B_MUTATION_BOUNDARIES.md` are normative. The CMOS method is **CMOS-T1,
PROPOSED — UNVERIFIED**: reconstruct cells60..68 from a complete known boot write
sequence plus continuous independently calibrated tracing and a pinned RTC model
whose non-guest transitions cannot alter them. No guest observer reads/writes or
invented emulator export is used. Missing model/coverage proof blocks equality.

Writer oracle **W** combines exact-build source/CFG forbidden-edge analysis,
bounded independent execution-site observation and device request evidence.
Zero ATA events or unchanged image bytes alone cannot prove that a forbidden
writer API was never entered. Same-value CMOS/media writes still fail.

| ID | Open evidence / consequence |
| --- | --- |
| U-BUILD | Future fixture source, ELF/hash, numeric UD2/checkpoint/caller/destination bindings and arm addresses do not exist. Review after separate implementation authority, before any run. |
| U-RAM | Future exact stack high-water/store coverage, fixture allocation and complete phase-specific bit/byte masks not derived. Budgets are specified, not measured or proved sufficient. No run until independently reviewed. |
| U-CMOS | Exact RTC model/source-to-binary coverage attestation and continuous-trace calibration absent. CMOS-T1 cannot yet establish new-case equality. |
| U-WRITER | Future executable forbidden-edge registry, coverage/breakpoint-capacity proof and entry/I/O calibration not implemented. No no-call claim for a new case. |
| U-DEVICE | NC/ND exact device tree/launch/status behavior and profile-specific sensor calibration unprovided. These profiles remain deferred. |
| U-P2 | Future READ checkpoint/buffer/ownership bindings and model phase/status evidence unprovided. P2 remains later gated work. |
| U-UART | Returning deterministic THRE-busy model/counters and separate absent/discard readiness oracle absent. U profiles deferred. |
| U-P3 | Exact submission/data/flush/completion model, sector payload, baseline and atomicity ledger absent. No write-delta permission. |

These are explicit gates, not requests to implement/calibrate them in this run.
H0-Core's written finite data catalogue is ready for independent design review.
A1-UD-1020 is a pinned design-review candidate, **not execution-ready**. The
authorization ladder contains no combined H0+A1+P1+R1 grant.

## 8. Deferrals and bounded future-architecture compatibility

Defer A2's four-byte mapped-reservation crossing; additional A1 parameters;
R2/R3/P2 execution; U1 absent/discard, U2 busy, U3 final-poll/partial output;
R4 secondary fault inside completed DF, R5 distinct sequential PF/CR2 challenge;
U4 combinations and P3 active writes. Earlier-in-code is not earlier-in-risk.
Also defer special-SS/#SS descriptors, pre-latch races, failed DF delivery, repeated
DF into a busy TSS, mid-memcpy/auxiliary-publication faults, third-fault storms,
higher-level transaction/CMOS gaps, corrupted SS/DS/CR3/task metadata, recursion,
NMI/machine-check storms and physical-silicon claims.

Preserve profile separation: O and R-N have TR28/live NT0; uninterrupted D has
TR30/live NT1/hardware TS; later R-D has TR30/live NT0 but saved secondary NT1.
DF record IP/CS/flags/ESP originate in verified previous TSS, while early observed
CR0/CR2/CR3 belong to the new task. No corrupted-root rescue is demonstrated.
Assembly reentry has no software stack pushes/C calls but still requires its
hardware frame and accessible data/I/O. Its auxiliary slot is not a general
tertiary-fault atomic publication protocol.

The AI-native/native adaptive execution philosophy remains future research.
Kernel enforcement, ordinary process isolation and independent correctness/
performance evidence remain authoritative. A proposal from intelligence is not
execution permission or proof. Critical fault paths cannot depend on an AI/END
service; KamikazeOS and The END Engine/Fibi remain separate projects. QEMU is the
testbench, not a mandatory native-application VM policy. Later user-space dynamic
code, reversible optimization, scheduling and hardware profiling need their own
authorization; Stage 2B neither implements them nor freezes current static layout
as permanent OS policy. Current global emergency/TSS/stack ownership is single-CPU;
it is not SMP/per-process capability. No Phase C/D, Ring 3, WP/NX, guard activation
or adaptive-execution implementation is included here.

## 9. Revision conclusion

F01/F03/F06 have explicit corrected design contracts. F02/F04/F05 have corrected
rules but retain identified measurement/build prerequisites and are **PARTIALLY
RESOLVED**. Every new case remains UNVERIFIED. See the finding dispositions and
preservation result before considering any later proposal.

**REVISION READY FOR INDEPENDENT REVIEW**
