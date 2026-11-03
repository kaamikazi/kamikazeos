# Stage 2B Oracle And Mutation Audit

This is static review of proposed predicates. Necessary/sufficient below means
for the named bounded case under its explicit architectural/model assumptions,
not a general proof about arbitrary hardware or faults. All new outcomes are
UNVERIFIED / NOT RUN.

## Case-By-Case Oracle Quality

| Case | Necessary mechanism evidence | Sufficient only together with | Independent observability / falsification | Present status |
|---|---|---|---|---|
| H0 HP/HN catalogue | Complete fixed row IDs/input mode and independent expected classifications. | Actual input/schema hashes, reference byte/CFG binding and all positive/negative expectations checked; synthetic outcome cannot masquerade as hardware. | Data interpreter can observe fields it is given; independent fixed expected outputs needed. Missing parser/input/corpus cannot count as negative PASS. | Suitable H0-I design; H0-C inputs/implementation NOT DEMONSTRATED. |
| A1-UD-1020 | Vector6 at actual inline UD2, source12 geometry and raw frame, validated source/origin, independent real healthy storage/task/CPU state. | Exact0x04/eligible0 primary, O halt, whole-source/unrelated-RAM preservation, W/CMOS qualified, deadlines/trace complete. | CPU/raw physical memory plus bound machine code independent of serial. DF, extra unsafe cause, source delta or wrong halt FAIL. Missing qualified observer UNVERIFIED. | F01 semantic correction sound; A1-E not ready. |
| P1-NC | Real unmapped primary ports, first statusff before IDENTIFY; valid frame and only BAD_STORAGE. | No forbidden entry, complete predecode port-attempt coverage, RTC chain, proper no-media N/A, O halt and RAM preservation. | Absent-device callback cannot observe an OUT attempt; needs independent instruction/port coverage. Any writer early return FAIL. | U-DEVICE/U-WRITER/U-CMOS open. |
| P1-ND | Real primary controller with no master, actual ec then00 identify branch. | Same no-entry/CMOS rules and correct unavailable trust tuple; no media equality inference. | IDE command activity proves controller, not write calibration or no API call. Healthy attached-device reference required for applicable formats. | Deferred; exact topology unprovided. |
| P1-LK | Readable actual seed, same-generation unequal valid states, genuine initialized locks/trust0. | Source/entry no-writer witness, zero new commands/data/flush, CMOS reconstruction, O halt, full settled image equality. | API entry sensors catch calls that lock guards suppress; writer/CMOS event FAIL even when disk unchanged. | Seed premise established; actual copy/calibration NOT DEMONSTRATED. |
| R1-UD-EMIT0 | Eligible1/unsafe0 published parent, auth0, first ordinary-primary emitter caller/args, actual second UD2/frame. | Exactly[6,6], counter1/auxiliary, no parent prefix, parent128/entry100 frozen, R-N proper stack and full writer/CMOS window. | Observed tuple and raw CPU/site/frame discriminate actual secondary from generic emitter or debugger STOP. Wrong caller/denied parent/auth1 or shortcut FAIL. | Semantic tuple resolved; live C-SEC and sensors absent. |
| R2-PRE-ESP | Exact pre-MOV boundary, latch1 and partial100/primary0, actual second UD2 on original stack. | Original parent frame preserved, new adjacent12 below it, current bootstrap ESP-12 terminal, no C/software-stack activity. | Current/raw ESP plus fixed-state bytes; terminal fatal-top ESP or extra store FAIL. | Semantic split resolved; future hook/bounds unverified. |
| R2-POST-ESP | Post-alignment/pre-first-CALL boundary, current ESP=fatal top, same partial state. | Secondary12 at fatal_top-12, frozen parent source frame, no CALL/store/recapture. | Must distinguish before versus after CALL's return-address push. Post-CALL substitution FAIL. | Separate later case; no execution. |
| R3-PRE-MAGIC | Exact primary destination/ESI, completed128 copy and cleanup, magic0 before selected store. | Bytes remain unchanged after secondary, counter/auxiliary/R-N correct. Published semantic validity is explicitly not established. | Physical pre/post128/100 plus compiled data flow; mid-copy/generic memcpy or repaired magic FAIL. | Byte-only design valid; future binding absent. |
| P2-READ-CMD | Actual owned READ0x20/count1/LBA2047 after OUT before guest poll/INW, depth1/zero words. | Known model phase/status, raw source frame, unsafe0x30/eligible0/O, owned buffer unchanged and no new request/writer/CMOS. | Operation identity and actual transfer history; depth alone or wrapper pending cannot pass. Backend read may already be complete. | U-P2 open. |
| A2 | Exact separate four-byte mapped-reservation crossing. | Independent mask/nonownership/raw frame and new specific authorization. | Adjacent corruption outside declared4 FAIL; no active-guard claim. | Explicitly deferred, no current mask authority. |
| U1 absent/discard | Actual device profile and serial-independent initialized readiness. | Coverage/bindings for boot/arm and applicable failure model, preserving historical healthy serial rules. | Empty UART output does not establish boot, busy polling or safe halt. | U-UART open. |
| U2/U3 busy/partial | Returning deterministic THRE model, per-physical-byte/poll counters. | Separately qualified route/limits/deadlines and independent lossless counts. | Nonreturning host I/O is not bounded by guest polling; timeout not PASS. | Deferred; no model/counters supplied. |
| R4/R-D | Completed DF parent plus later genuine event and raw savedNT1/liveNT0 distinction. | Frozen DF/TSS/parent state, correct R-D task/stack and no repeated busy task-gate delivery. | Generic R-N/D relaxation must not accept; expected[14,8,6] differs from task return. | Deferred; no readiness granted. |
| R5 | Two genuine sequential PF sites/addresses with completed first delivery/publication. | Raw/trace/primary/aux CR2 provenance and one later entry; not PF delivery overlap. | [14,14] distinct from[14,8]; stale CR2 text insufficient. | Deferred. |
| P3 WRITE/FLUSH | Exact frozen operation/submission/data/flush/completion model and sector payload. | Explicit predeclared atomicity/delta ledger, unrelated-sector immutability and separate grant. | Never choose old/full/partial permitted bytes after observing outcome. | No present acceptance predicate or write-delta authority; deferred. |

No negative outcome is erased or repaired. Each future instance needs its own
directory and retained raw bytes before any repeat or observer revision.

## Exact R1 Parent/Checkpoint/Admission Audit

The previous problem was semantic, not just missing numeric labels. Revision1
now freezes the existing ordinary invalid-opcode trigger, a healthy bootstrap
parent and first primary-emitter entry before its original prologue. Production
machine code shows:

```text
0x00102e58 CALL em_record_capture(primary,...)
0x00102e62 PUSH context0
0x00102e64 PUSH console_put (0x00102f20)
0x00102e69 PUSH primary (0x0013dbc0)
0x00102e6e CALL em_record_emit (0x001037e0)
0x00102e73 caller return / stack cleanup
0x00102e8a STORE ordinary_authorized
```

At entry `[ESP]` is0x00102e73 and the three arguments occupy+4/+8/+12.
Published primary128 and decoded entry100 already exist. Ordinary authorization
is still0, so eligibility1 must not be interpreted as current permission1.
Healthy trust/depth0 and full prospective secondary frame containment are real
requirements. A generic DF emitter/denied parent cannot satisfy this tuple.

C-SEC does not exist in production. Its future nonspilling two-DWORD prefix,
flag effects, UD2 address and exact caller witness remain U-BUILD. No actual
compiled secondary checkpoint is certified here. The tuple is a falsifiable
semantic specification, not an executed event or owner authorization. Prior R2
execution is unnecessary; R1's own delivery/publication/fatal-stack data suffice
if its observation gates are actually established later.

## RAM Domain And Delta Audit

| Class | Address/derivation, owner and epoch | Allowed effects | Forbidden effects / observer limit |
|---|---|---|---|
| Hardware source frame | A1 `[B+1020,B+1032)` from future linked bootstrap bounds; baseline E-fault after pivot/pre-UD2/IF0. | Exact saved EIP/CS/EFLAGS DWORDs; for secondary cases the specifically named12-byte interval below current ESP. | Extra bootstrap/old-frame/reservation byte, wrong width or task tail FAIL. Physical pre/post reads plus CPU/site evidence; cannot infer transient stores from equality alone. |
| Fixed entry state | P `[0x13db50,0x13dbb4)`100 bytes, rebased from exact symbol ABI in future ELF. | First-entry assembly stores and decoder updates; R2 explicitly partial; frozen all100 at E-parent for R cases. | Recapture or changed parent state after secondary forbidden. Correct serial alone cannot prove its bytes. |
| Crash slots/latch | Primary `[0x13dbc0,0x13dc40)`, DF `[0x13dc40,0x13dcc0)`, aux `[0x13dcc0,0x13dd40)`, counter at0x13dd40, busy0x13dd44, authorization0x13dd48, ready0x13dd4c. | Primary body/magic-last on first entry; busy0->1; secondary aux only offsets0,4,8,12,16,24,40,44 and counter0->1. | DF/aux0 for A1, frozen primary128/entry100 for R; all other aux bytes0, no high busy bits or reset. Record field predicates constrain permitted values, not arbitrary128-byte allowance. |
| Diagnostic stack | Proposed `[fatal_top-4096,fatal_top)`; P `[0x15d000,0x15e000)`, independently enumerate all pushes/calls/spills/stores. | Only reviewed stack operations within proven maximum depth; strict secondary epoch excludes all software stack stores. | Whole16-KiB or arbitrary-pointer exemption invalid. Present 4096 cap is not high-water proof; U-RAM unresolved. |
| Fixture | Future disjoint aligned arm/ack DWORDs,8 bytes; P2 later owned512-byte PIO buffer. | Exactly declared arm/ack transitions before strict snapshot; P2-CMD buffer unchanged. | Actual addresses absent, no production flag/readiness/counter forgery, no debugger memory patch. Unknown allocation/overlap blocks. |
| CPU/task metadata | Future enumerated PDE/PTE/GDT word locations, source-symbol/page-walk derivation; O/R-N no task switch. | Enumerated PDE accessed0->1; PTE accessed and justified-write dirty0->1; named descriptor accessed0->1. | No broad page mask; TSS/backlink/busy/DF globals immutable in O/R-N. Missing actual word/bit list leaves acceptance UNVERIFIED. |
| Unrelated memory | All observed normal-RAM complement of the above, excluding separately accounted ROM/MMIO domains. | Endpoint byte equality, unchanged old live frames and other owners. | No BSS/heap/reservation/global exception. Uncaptured RAM cannot be silently excluded. Host reads of MMIO can have side effects and are not admitted as ordinary RAM inspection. |

The revision's six operational classes cover the requested seven distinctions:
entry and crash records are separate subcontracts within M2; source, stack,
fixture, architectural state and unrelated memory have separate classes.
Read effects are allowed only through qualified host physical reads of normal
RAM. Hardware writes to page/descriptor state require exact per-word coverage.

E-live (after CLI/arm, healthy stack) and E-fault (post-pivot) are both necessary.
PIT/keyboard/command work before CLI belongs to the pre-window baseline, not
unapproved deltas. IF0 removes maskable IRQ delivery; it does not account for
all NMIs, DMA, model callbacks or host activity. One CPU, fixed topology, no
migration/reset/patching and qualified device/trace windows remain assumptions
to verify at future execution review. Unexpected contamination cannot be
explained away by expanding a mask after the fact.

## Operational CMOS Chain

Cells0x60-0x68 are not ordinary RAM. The actual kernel selects index at port0x70,
waits and accesses data at0x71. `recovery_memory_save` writes the nine-byte block
unconditionally before conditional disk persistence. A healthy boot includes
legitimate writes before E-boot; unsafe-window writes to any index are forbidden.

CMOS-T1 is adequate as a design hypothesis only if all links are established:
exact emulator/model/source-to-binary identity; exhaustive cell-update paths;
full known overwrite and checksum/magic/version reconciliation; ordered actual
index/value events; same-value/all-index positive sensor calibration; independent
continuous loss accounting through E-close; no hidden autonomous recovery-cell
update/reset/migration. Initial bytes plus no observed writes establish equality
only under these qualified premises. Source code and old boot events do not
prove continuous logging stayed active in a future window.

The retained accepted traces support the historical reconstructed block and no
recorded post-activation RTC writes. They do not establish model coverage. No
direct CMOS snapshot, QOM field, guest read/index-select or NMI-mask modification
was used here. Non-guest RTC clock/status evolution must remain separate. A
hidden-update model, trace gap, incomplete overwrite or observer failure is
UNVERIFIED, not equal-by-disk-hash. This is F04 PARTIALLY RESOLVED.

## P1 Causal No-Writer Distinctions

| Causal state | Necessary discriminant |
|---|---|
| Writer edge never entered | Exact-build forbidden-edge/inline/indirect registry plus complete nonpatching entry/execution-site observation; negative result only with positive qualified coverage. |
| Writer entered, lower guard returned | Entry hit is FAIL even if ATA trace0 and image unchanged. Store/journal/ATA source contains this possible early-return pattern. |
| ATA activity occurred | Command/data/flush issue events, ownership and event timing independent of final image hash; same-payload writes still FAIL. |
| Preowned operation completed later | Observe the actual submission and operation identity before onset and completion after it; classify per case. P1 demands idle depth0 and settled healthy/absent/locked baseline, so it has no permitted pending WRITE/FLUSH. P2 separately permits completion of its one read. |

The W registry has only a proposed <=16-site ceiling, not an implementation or
coverage proof. All inline/merged/indirect paths must fit simultaneously without
rotating breakpoints through blind windows. Positive per-entry/site and request
calibration cannot be replaced by synthetic declarations. This remains U-WRITER.

NC requires actual no-controller/predecode port-attempt coverage; a nonexistent
device cannot emit its callback. ND requires controller ec/status00 evidence.
LK requires actual valid unequal states and locked/readable journal state. All
require CMOS measurement; disk N/A is no proof of denial. The actual seed's
unchanged bytes verify boot1/generation5/jcount3 and matching state copies.
The recipe predicts equal-generation conflict, RAM generation0, valid0/locks1,
scan/order1 and diagnostic boot2 with invalid initial CMOS. No conflicting
copy was created, and this predicted tuple was not experimentally checked.

## Verdict And Failure-Preservation Discipline

The design generally separates PASS, FAIL and UNVERIFIED. Require the following
implementation distinction at every gate:

- An actually observed wrong vector, writer hit, forbidden delta, reset or
  executing wrong halt route contradicts an invariant: FAIL.
- A complete healthy observation window exceeding the declared behavioral
  deadline may FAIL its deadline. A timeout caused by missing/dropped observer
  evidence is UNVERIFIED regarding the guest; neither is PASS or progression.
- No serial output, missing publication, a breakpoint alone, unqualified empty
  ATA trace or unavailable CMOS observation cannot satisfy an invariant.
- Fixture/parser/schema failure must be identifiable separately from guest
  mechanism failure. Preserve the original input and first failed receipt.
- Freeze raw offsets, epochs, whole input hashes, masks and limits before the
  case. Observer changes require a new reviewed version/fresh instance; failed
  evidence is never overwritten or retrospectively classified successful.

The ladder enforces separate H0, A1, P1 and R1 authority. Design review is not
implementation authority, owner authority is not execution, and execution is
not PASS. The open U gates block the respective later decisions.
