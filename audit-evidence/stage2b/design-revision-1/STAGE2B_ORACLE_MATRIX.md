# Stage 2B Revision 1 — oracle matrix and control specification

**All entries are expected design outcomes, not results.** H0 controls, observers,
fixtures and calibration described here were neither implemented nor run. New
runtime behavior is UNVERIFIED. Source references use K/P as defined in the main
revision. Mutation classes/epochs are defined in `STAGE2B_MUTATION_BOUNDARIES.md`.

## O0. Verdict and evidence contract

- **PASS:** every approved mechanism, raw event/site/frame, admission/publication,
  task/stack/halt, writer/request/state, mutation, deadline and preservation rule
  is positively established. All required sensors have independent coverage proof.
- **FAIL:** an observed violation, including unexpected DF/rescue, replaced record,
  writer entry/idempotent request, wrong terminal, continuation, reset/triple fault,
  timeout or unapproved delta. Preserve raw failure, stop progression.
- **UNVERIFIED:** a prerequisite/coverage/binding is absent or ambiguous; dropped
  trace, unavailable baseline/model status or a debugger-only stop cannot prove
  the required mechanism. Stop before trigger if known then; otherwise quarantine
  the incomplete run. It never counts as PASS or permission for the next case.
- Copied magic0 bytes can satisfy a **byte-preservation subclaim** in R3 without
  satisfying published-record validity. The latter remains UNVERIFIED.

Mandatory evidence is the immutable source/ELF/model/config/observer manifest,
ordered QMP and debugger records, raw CPU exception/reset trace (i=0 guest faults),
serial/I/O stream, per-epoch CPU/RAM/frame/descriptor/TSS captures, full isolated
disk copies and CMOS-T1 state/coverage receipts. Log routing must be positively
checked: the historically accepted QEMU backend can place CPU output in the
`-trace` destination instead of the apparently separate `-D` file. Stream presence
and immutable offsets are required; silence/empty files cannot pass.

### Route profiles

All require CPL0/CS08/SS10/data10, expected CR3, IF0, exact-build CLI/HLT/backedge,
HLT1 and stable PC/ESP >=250 ms apart, within unchanged60s/30s deadlines. Arithmetic
flags are decoded per path, not copied from an unrelated checkpoint. No IRET,
normal/task return, firmware execution or host shutdown/paused-state substitute.

| Profile | Task/stack/NT | Terminal and content requirement |
| --- | --- | --- |
| O | TR28, fatal payload and reviewed4096-byte budget, live NT0 | Published ineligible primary; unsafe branch only. P post-HLT `0x00102ec2`; dispatcher fallback `0x00102eb2` rejected. |
| R-N | TR28, current checkpoint stack minus secondary hardware frame, live NT0 | Counter1, sparse published auxiliary, frozen parent/entry, no software ESP change; P reentry post-HLT `0x001001c7`. |
| D, historical reference | TR30, DF payload, live NT1, expected hardware TS | Unchanged accepted Stage 2A entry/TSS/backlink/busy/record/serial/terminal checks (62+4). No new runtime result. |
| R-D, deferred | TR30, DF payload minus secondary frame, live NT0; saved secondary NT1 | Completed DF parent immutable, auxiliary/counter1, exact reentry terminal; never accepted by merely relaxing D. |

P offsets are reference evidence only. New builds must bind these routes by actual
control flow and caller/destination, including optimized/inlined halt sites.

## O1. Case-specific positive and falsification matrix

| ID | Positive predicates beyond O0 | Observed violations that fail | Missing proof that blocks / leaves UNVERIFIED |
| --- | --- | --- | --- |
| A1-UD-1020 | Exactly[6]; raw12-byte frame at B+1020, origin B+1032; valid frame1/origin1, kind1/context1, unsafe_flags=0x04 only/eligible0; runtime/storage1/depth0; O and full healthy primary block. | Frame invalid, extra unsafe reason, eligibility1, unexpected DF/reentry, forbidden formatter/writer, source/old-frame delta or fallback HLT. | Post-pivot baseline, true live-frame nonoverlap, no-spill UD2 binding, U-RAM or CMOS/W coverage absent. |
| P1-LK-UD | Exactly[6]; valid frame/origin, unsafe_flags=0x10 only/eligible0; actual seeded ambiguity, locks1/1, valid0, scans1/1, RAM generation0/jcount3, trust0; O; full image equal. | API entry suppressed by a lower lock still fails; seed repaired, wrong trust, extra rejection cause, any new request. | Actual seed hash/initialized tuple, no-call coverage or qualified CMOS missing. |
| P1-NC-UD | Same valid ordinary result10/eligible0; real no-controller statusff before IDENTIFY, initialized unavailable tuple, O; disk=N/A. | Present disk/controller branch substituted, writer entry despite zero device events, forged flags or CMOS fallback writer. | Exact topology/status/port-attempt calibration absent (U-DEVICE/U-WRITER). |
| P1-ND-UD | Same ordinary result; real controller/no primary-master, ec IDENTIFY then status00 branch, unavailable tuple; disk=N/A. | No-controller substituted, healthy/different device, lower guard hid writer, boot activity put inside fault window. | Controller/model/tree and no-call/CMOS coverage absent. |
| R1-UD-EMIT0 | Exactly[6,6] at named parent and C-SEC; primary published/eligible1/unsafe_flags=0, storage1/depth0, auth0 and qualified ordinary first-emitter caller/destination; frame12 on F, R-N, parent128/entry100 unchanged. | Wrong/ineligible parent, generic/DF emitter, postauth/later-byte site, debugger trap or direct formatter shortcut, counter0/2, record change, recursive C, writer or resume. | Unique linked hook and args, pre-secondary snapshots, genuine secondary raw frame/site or complete writer window missing. |
| R2-PRE-ESP-UD | Parent delivered, latch1, all common assembly stores complete, no C decode; parent primary0; secondary on current bootstrap ESP−12; R-N and exact partial bytes preserved. | Fatal-top terminal, before-latch point, C capture/publication, wrong partial entry fields, extra source write. | Exact pre-MOV witness or hardware-frame capacity/nonoverlap absent. |
| R2-POST-ESP-UD | Same partial entry; postalignment/pre-CALL current ESP=Ftop; secondary frame Ftop−12; primary0 and no C resumes. | Treating a post-CALL point as same variant, stack reset to top, capture/publication. | Exact postalignment/pre-CALL witness absent. |
| R3-PRE-MAGIC-UD | Destination primary/ESI, body copy and cleanup complete, magic0; snapshot128 preserved magic0, entry100 unchanged; real secondary and R-N. | Wrong memcpy/destination, mid-copy, already-published or later repaired magic, interpreting body as valid published record. | Exact data-flow/store-boundary witness or full pre-image absent. |
| P2-READ-CMD-UD | First owned READ0x20/count1/LBA2047, depth1, no retry, zero words transferred at exact post-OUT; valid ordinary frame/origin; unsafe_flags=0x30 only/eligible0, O; image/buffer equal. | Forged depth, wrapper-only interval, new command/data write/flush/CMOS, buffer change, unrelated operation, claiming0x20 alone as correct flags. | Operation identity, phase/model-status coverage, exact buffer/ownership and hook binding (U-P2). Backend pending is not required or claimed. |

For all unsafe cases, an unchanged disk/CMOS block without qualified no-request
and no-call evidence is insufficient. The CMOS and writer chains below apply to
every row, even where hardware absence could suppress physical media activity.

## W. Forbidden writer-edge oracle — PROPOSED / UNVERIFIED

### W1. Causal source and exact-build proof

K `emergency.c:108–114` captures/emits, computes authorization, then halts if denied.
R1 interrupts before authorization; its assembly reentry has no C call/return.
O's denied branch and R-N's stackless terminal must have no path to ordinary
dispatch/formatting or persistence. Bind this complete reachable graph in the
future linked ELF, including local labels, indirect sink target `console_put`,
inlined/merged helpers and tail calls. An unresolved indirect edge blocks W.

Explicit forbidden families from first-fault onset:

1. `interrupt_dispatch`, `kpanic_exception`, `panic_ordinary` and the actual inlined
   `record_panic` gate/call sites. Direct authorization-failure jumps to the reentry
   halt in `panic.c:76,115` are not evidence of a secondary CPU fault.
2. `recovery_memory_mark_panic`, `recovery_memory_save` and any inlined CMOS write
   sequence. Save writes cells60..68 **before** its conditional disk call
   (`recovery_memory.c:75–114`); disk absence does not remove this edge.
3. `recovery_store_save`, `recovery_journal_append`, new `ata_read_sector` entry,
   `ata_write_sector` and `write_sector_once`, including clones/inlined bodies.
   Store/journal/ATA validity checks can return before device I/O; entry is already
   a violation, irrespective of lower return values (store483–486, journal581–591,
   ATA246–263).

Other normal writers (snapshot, slot/package/TinyFS/clear/action mutators) must be
statically unreachable from the selected emergency graph; no open indirect edge
may hide them. Device-level observation catches unexpected port requests as well.
This is a proof about the selected route, not a universal barrier enforced by all
lower APIs against arbitrary out-of-band calls.

### W2. Bounded independent execution-site coverage

Before any future unsafe run, enumerate a **finite, hashed registry of <=16 exact
PCs** covering all forbidden function entries, inline first-body sites and call
edges in that build. Record ownership, bytes, caller/data-flow qualification and
window. Observation must be independent of the guest's own claims and active
continuously from parent onset through terminal. Any hit is FAIL even if it would
immediately return without an OUT. Source helper names alone are insufficient.

Use only separately reviewed nonpatching execution observation. Prove the selected
debugger/trace observer can cover the entire simultaneous registry within limits;
do not assume unlimited Z1 breakpoint capacity or rotate sites with blind gaps.
If16 sites are insufficient, coverage/capacity cannot be proved, or the actual
build removes/merges a site, U-WRITER remains open and the run is blocked. No
breakpoint/observer code was installed or tested here.

Independent calibration must positively demonstrate each covered entry/site class
on permitted healthy work or host-data routing controls, with same source/build/
observer and applicable model identities. Where a genuine healthy guest entry is
needed it needs separate calibration-execution authority. A synthetic control
checks interpretation only; it cannot establish live breakpoint/model coverage.
No direct writer invocation is injected into an unsafe case for calibration.

### W3. Per-profile request sensors and applicability

| Profile | Required positive evidence / healthy reference | What must not be inferred |
| --- | --- | --- |
| A1 / R1 / attached healthy controls | Same-instance boot READ/IDENTIFY and complete CMOS writes; separately authorized healthy reference positively exercises IDE WRITE/data/FLUSH and CMOS sensors. Verify exact model/observer identity match. | Historical trace from another configuration/build is not automatic new calibration. |
| P1-LK | IDENTIFY and successful state/journal reads in this boot, exact ambiguity RAM; known boot CMOS writes. Healthy same-model reference for media-write/flush sensor classes, W registry coverage mandatory. | No write events while locked cannot prove API nonentry. |
| P1-NC | Proved unmapped primary ports and real CPU ff-read/early return; independent **pre-device-decode port-attempt** observation or complete equivalent instruction/graph coverage; positive RTC writes. Healthy controller reference qualifies IDE formats, not absent decode. | A device event callback on a nonexistent controller cannot detect an attempted OUT to that absent device. No healthy media-write positive in this boot is required or possible. |
| P1-ND | Proved controller topology, ec command/status00 in the actual stream, W entry coverage; positive RTC writes. Healthy attached-media reference separately qualifies media WRITE/FLUSH. | No disk cannot generate a healthy media-write completion; N/A is not PASS for that sensor claim. |
| P2-READ-CMD | Exact pre-fault READ identity/event order, same CMOS/W controls; model phase/status evidence with zero guest transferred words. | Positive depth does not prove backend pending. A prior READ completion is not a new handler command. |

Missing applicable calibration produces UNVERIFIED. No artificial unsafe-window
writes or assumed emulator behavior fill a gap. U-WRITER/U-DEVICE track the live
coverage currently absent from this design-only revision.

## CMOS-T1. Recovery state reconstruction — PROPOSED — UNVERIFIED

### C1. One chosen measurement chain

Use **complete known guest writes + continuous calibrated RTC tracing + audited
pinned RTC model**. No guest CMOS-reading observer is proposed, no QOM property or
unverified save-state export is assumed, and no index-select/read command is
issued by the host through guest code. Host inspection therefore introduces no
port70/NMI-mask or read-side effect. Existing legitimate boot I/O remains visible.

Required chain, before equality can be asserted:

1. Pin exact emulator binary, source/build correspondence, machine/RTC model,
   firmware, launch flags and trace-event definition. Audit every path that can
   mutate RTC recovery bytes60..68: guest callbacks, initialization/reset, timers,
   migration/load, device callbacks and host management. Prove no autonomous or
   hidden transition touches these cells during the allowed run. Initialization
   before the known overwrite is immaterial; resets/migration/model changes after
   it are forbidden. Clock/status updates in00..0d are separate.
2. Enable tracing **before boot**, retaining the complete raw prefix through final
   stop/flush. Prove the selected write event records the effective cell index and
   value for every guest data write, including same-value writes; check any bank/
   alias/index-mask behavior. A port70 index select is not itself a cell update.
   Do not assume an event labelled ioport_write is already coverage proof.
3. Identify a full nine-cell kernel save in settled boot. Reconstruct the last
   actual values of every cell60..68 by applying all intervening writes in order.
   Require magic4b/5a, version01, flags derived from independently captured RAM,
   boot/last-action/prior-status bytes and additive checksum0. Expected flags are
   safe_next1 | net_quarantine2 | audio_quarantine4 | last_panic8 |
   derived_rollback16 | (last_panic && intentional)32. Do not infer current CMOS
   from a RecoveryMemory pointer alone. Freeze raw event offsets and the nine-byte
   reconstructed state at E-boot/E-fault (or parent fault for R1).
4. Establish continuous coverage, independent of the guest: trace configuration,
   positive events for all nine cells/values, event/counter accounting and no gaps,
   dropped/truncated buffers or routing changes. Separately authorized healthy
   calibration must demonstrate that legitimate writes (including idempotent ones)
   are detected and index/value decoding matches audited callback semantics. A
   pinned lossless capture/counter witness must span the unsafe window; merely
   seeing old boot events is not proof that logging remained active later.
5. From first-fault onset through E-close require **zero guest CMOS data writes to
   any index**, not merely zero changes of60..68. Apply every event, if present, to
   the reconstructed map: any write is FAIL, including same-value. Under the
   audited no-autonomous-change invariant and continuous zero-write coverage,
   final reconstructed60..68 equals its frozen initial value. Record the result
   explicitly as **reconstructed equality**, not a direct physical snapshot.

If source-to-binary correspondence, model coverage, known initial writes, checksum,
window order, counter integrity or no-hidden-writer proof is absent, equality is
**UNVERIFIED**. This is a run-blocking prerequisite, not permission to drop CMOS
equality or replace it with a disk hash. A trace gap cannot be repaired by a later
matching state, and a zero-write count without calibrated coverage cannot pass.

### C2. Existing evidence and calibration boundary

`EXISTING_CMOS_TRACE_DATA.json` statically reconstructs accepted Stage 2A cases'
last pre-trigger cells as `4b 5a 01 00 01 00 00 00 59`, with zero recorded
post-trigger recovery writes. It explicitly records **no model source/binary
coverage attestation**. These are historical trace data, not fresh snapshots,
new calibration, or proof the future RTC model satisfies C1.

U-CMOS remains open. H0's data catalogue below can check reconstruction/rejection
logic under declared mock coverage; it cannot calibrate a real RTC. Any later
live calibration is a separate owner decision, not part of H0-Core authority.

## H0-Core. Finite host-data catalogue (specification only)

H0-Core is limited to pure interpretation of immutable reference bytes/CFG and
explicitly synthetic tuples/streams. No guest hook, VM, device control, guest-memory
write or new fault is part of H0. This revision writes **no executable controls or
control-input fixtures**; the following56 rows are the frozen textual catalogue.

All synthetic inputs carry `evidence_kind=SYNTHETIC_CONTROL`, a distinct mock
binding identity and known field/byte relations. They are never output as hardware
acceptance. The production/accepted-ELF reference modes independently check actual
bytes/CFG from the hash-pinned inputs; synthetic mock UD2/checkpoint bindings must
never be reported as instructions already present in P.

Common synthetic template: bootstrap B/T and fatal bounds equal the P reference;
CS08/SS/data10/TR28, CR3=`0x00148000`, CPL0/IF0/NT0, PG on/WP clear/CR4zero;
normal raw frame ESP=B+8192, origin=+12, diagnostic ESP=Ftop−256; R-N secondary
frame `[Ftop−268,Ftop−256)` unless its row overrides. EIP is the row's declared
mock UD2 and exact raw bytes/flags agree. Primary/entry have the specified exact
128/100-byte expected images, auxiliary startszero, latch/counter0 before first
event, stable halted samples300ms apart, no reset, valid mock sensor attestations,
equal image/CMOS and frozen M1–M6. Mock flag/frame data is internally explicit;
actual future flags/bytes require U-BUILD, not the template. Each negative changes
only its stated predicate from its applicable positive unless otherwise stated.

### Eight positive data controls — expected ACCEPT in control mode only

| ID | Intended accepted tuple / bounded claim |
| --- | --- |
| HP01 | A1: S=B+1032/rawESP=B+1020/frame12, published magic, frame_valid=origin_valid=1, unsafe_flags=0x04/eligible0, all other admission predicates true, O. |
| HP02 | P1-LK: healthy raw frame; real-profile mock ambiguity tuple, unsafe_flags=0x10/eligible0, no forbidden entry or request, complete mock CMOS chain, O. |
| HP03 | R1: eligible1/unsafe_flags=0 parent, auth0, correct first-primary-emitter caller/args, zero parent-output prefix, genuine mock[6,6]/raw secondary12, counter1/auxiliary, R-N, immutable parent/entry. |
| HP04 | R2-PRE: current pre-secondary ESP=parent rawESP on boot; primary0/partial entry frozen; final ESP=rawESP−12, real secondary/counter1, R-N. |
| HP05 | R2-POST: current pre-secondary ESP=Ftop before first CALL; same partial state, final ESP=Ftop−12, R-N. |
| HP06 | R3: full copied primary body/magic0 and exact destination/pre-store witness; bytes remain identical, auxiliary/counter1/R-N. ACCEPT byte-preservation only; semantic publication remains UNVERIFIED. |
| HP07 | Hash-bound preserved healthy D evidence is accepted only as a **retained reference**, with its unchanged62+4/serial/task contract. Synthetic D variants are labelled synthetic, never a fresh Stage 2A result. |
| HP08 | P2-CMD: real-profile mock READ0x20/count1/ownedLBA, depth1, zero words, known model phase/status, unsafe_flags=0x30/eligible0/O; backend completion may already be true, buffer/image equal. |

### Forty-eight negative controls — expected never PASS

"Reject" means observed-contract FAIL in a case; "UNVERIFIED" means a missing
prerequisite/measurement. H0 succeeds only if it classifies these as stated and
never promotes them to a hardware result.

| ID | Single discrepancy / expected classification |
| --- | --- |
| HN01 | A1 raw frame16 instead of12 / Reject geometry. |
| HN02 | A1 frame_valid0 / Reject validity, not headroom success. |
| HN03 | A1 origin_valid0 / Reject origin. |
| HN04 | A1 unsafe_flags=0x14 instead of0x04 / Reject masked storage failure. |
| HN05 | A1 eligible1 / Reject admission. |
| HN06 | A1 primary magic0 / Reject publication. |
| HN07 | A1 vector8/task-gate fallback / Reject mechanism. |
| HN08 | One source byte outside approved frame changes / Reject M1. |
| HN09 | One old-live-frame byte changes / Reject preservation. |
| HN10 | O stops at dispatcher-return fallback HLT / Reject route. |
| HN11 | Correct terminal PC but HLT0/running / Reject halt. |
| HN12 | O/R-N TR30 / Reject task. |
| HN13 | SS not10 / Reject inherited stack contract. |
| HN14 | CR3 differs / Reject root. |
| HN15 | R-N live NT1 / Reject flags/provenance. |
| HN16 | Halt samples only249ms apart / UNVERIFIED stability. |
| HN17 | Second sample ESP moves / Reject stability. |
| HN18 | New RESET/firmware/triple-fault identity after onset / Reject. |
| HN19 | CPU exception event stream missing, debugger stop only / UNVERIFIED event. |
| HN20 | RTC trace/window missing / UNVERIFIED CMOS. |
| HN21 | Zero events but no positive/continuous sensor coverage / UNVERIFIED. |
| HN22 | Idempotent CMOS data write, final cells equal / Reject request. |
| HN23 | Same-payload ATA WRITE/FLUSH, image hash equal / Reject request. |
| HN24 | R1 parent already ineligible / Reject selected parent. |
| HN25 | R1 ordinary_authorized1 / Reject phase. |
| HN26 | Generic emitter passed DF record / Reject destination. |
| HN27 | Primary pointer but wrong caller/return address / Reject checkpoint. |
| HN28 | Secondary SIGTRAP/STOP only, no guest vector/site/frame / UNVERIFIED secondary. |
| HN29 | Direct unauthorized-formatter shortcut to reentry HLT / Reject causal route. |
| HN30 | Reentry count2 / Reject exactly-once. |
| HN31 | Reentry count0 despite claimed secondary capture / Reject counter. |
| HN32 | One byte of R1 primary128 changes / Reject first evidence preservation. |
| HN33 | One byte of R1 entry100 changes / Reject entry preservation. |
| HN34 | Raw secondary saved EIP not the selected UD2 / Reject event/site. |
| HN35 | R3 magic published/"repaired" after its pre-store snapshot / Reject preservation/publication. |
| HN36 | R2-PRE halts at fatal-top-derived ESP / Reject selected stack phase. |
| HN37 | P1 writer entry observed, lower guard returns and zero ATA events / Reject W. |
| HN38 | P1 unavailable label with independently healthy/trusted target / Reject profile. |
| HN39 | P2 depth1 but no submitted READ identity / UNVERIFIED I/O mechanism; forged flag not admitted. |
| HN40 | P2-CMD at wrapper return with256 words transferred / Reject phase. |
| HN41 | RTC model permits unobserved autonomous recovery-cell changes / UNVERIFIED equality. |
| HN42 | Initial nine-cell overwrite incomplete or pre-window trace gap / UNVERIFIED initial state. |
| HN43 | Trace dropped/truncated/cap exceeded / UNVERIFIED coverage, stop. |
| HN44 | Synthetic/retained input presented as fresh hardware acceptance / Reject provenance. |
| HN45 | R1 C-EMIT0 has a nonempty parent emission prefix / Reject checkpoint; healthy D serial rules unchanged. |
| HN46 | R3 generic/mid-copy hit substituted for completed primary pre-store witness / Reject boundary. |
| HN47 | Prospective secondary frame does not wholly fit current payload / UNVERIFIED preflight; block trigger. |
| HN48 | Store/delta outside fixed M2 slot or inside an unapproved table bit / Reject ownership. |

The catalogue is finite (8 positives +48 negatives), with explicit expectations.
Later implementation must freeze actual input bytes/hashes and independently
review each relation; no result is recorded here. Backend/UART controls and live
CMOS/W calibration are outside H0-Core. If a mock coverage declaration is absent,
the appropriate missing-evidence negative applies; declaring mock coverage never
attests to a real model. Keep the original Stage 2A controls/assertions unchanged.

## Later device/DF/write cases

U1 absent/discard needs serial-independent, actual initialized CPU/RAM readiness;
no timeout-as-boot or null-as-busy inference. U2/U3 require a deterministic
**returning** THRE model and independent per-physical-byte/poll counts. Current C
emission budgets4096 polls/byte and1024 logical bytes imply a conservative
8,388,608 physical-poll ceiling; first-byte permanent busy stops within4096.
Assembly reentry differs: <=64 bytes ×4096 polls, independent of C presence.
Finite guest polling cannot bound host I/O that does not return. These are source
bounds, not observed UART results.

R4 must distinguish R-D live NT0 from saved secondary NT1 and uninterrupted D NT1;
R5 must distinguish sequential[14,14] from delivery-overlap[14,8] and retain four-way
CR2 provenance. Both remain deferred. P3 requires an exact frozen operation/model/
sector/data/flush/completion ledger before any image delta; no retrospectively
permitted partial/torn output. These later cases cannot pass through a relaxed
O/R-N/D profile or bootstrap the independent sensors they require.
