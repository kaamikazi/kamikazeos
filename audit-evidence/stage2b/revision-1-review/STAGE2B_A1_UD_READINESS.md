# A1-UD-1020 Authorization-Readiness Assessment

Verdict: **REVISE BEFORE AUTHORIZATION** for a fault-execution request (A1-E).
The mechanism is source-supported; missing exact-build RAM and independent
observation prerequisites prevent execution readiness. Nothing was run here.

## Independently Established Reference Geometry

| Item | Value | Evidence |
|---|---|---|
| Production ELF | SHA-2567cc1262fb1caee2b104d17d601a19e172eb02eac6d93b56ad2b2612a228db7bc | Actual file hash and independent ELF32 parsing |
| Bootstrap payload B/T | `[0x0014e000,0x00152000)` | Linker symbols and section extent;16384 bytes |
| Proposed initial ESP S | B+1032=`0x0014e408` | Design parameter; no fixture exists |
| Gate/frame assumption | CPL0, flat SS32, vector6 DPL0 interrupt gate0x8e | `idt.c:idt_init_exceptions/idt_set_gate_dpl`, GDT data selector0x10 |
| Hardware frame length | 12 bytes: EIP, CS, EFLAGS DWORD slots | Same-CPL non-error exception contract; no user SS:ESP tail |
| Final hardware ESP | S-12=B+1020=`0x0014e3fc` | Arithmetic over exact reference bounds |
| Written source interval | `[0x0014e3fc,0x0014e408)` | Wholly inside payload; adjacent low reservation not touched |
| Origin ESP | CPU ESP+12=S | `em_decode_exception`, has_error0 and bounded same-CPL frame |
| Source headroom | 1020 bytes, less than1024 | `EM_MIN_HEADROOM=1024`; ELF compare0x400 at0x00103567 |
| Synthetic PUSHA frame.esp | CPU ESP-8=`0x0014e3f4` | Decoder's historical convention; a stored value, not a write/followed pointer |

The current existing trigger at0x00107c30 has PUSH EBP/MOV EBP,ESP before UD2
at0x00107c33. Calling it after the pivot changes the frame geometry. The new
design correctly specifies an inline UD2 immediately after the fixture's ESP
pivot, with no call/push/spill/memory instruction in between. There is no exact
future label/address or linked proof yet; P's trigger is reference evidence only.

## Why Frame Validity And Persistence Denial Can Both Be Correct

`em_source_stack` checks alignment, known payload and complete frame containment.
It does not require1024 bytes of headroom to decode. `em_decode_exception` sets
frame_valid1, eflags_known1, CPU_FRAME context and origin_valid1 for exact CS08.
`em_persistence_unsafe` separately sets BAD_STACK if cpu_esp-stack_start<1024.
With all other context/storage/CPU conditions true, unsafe is exactly0x04 and
persistence_eligible0. `em_record_capture` still publishes the record magic last.

Thus **a perfectly healthy hardware frame-delivery mechanism can produce the
intended A1 result**. That is the point: policy headroom denial. It demonstrates
neither stack overflow, recursive fault recovery, active guard enforcement nor
an adjacent-page fault. The low reservation is mapped; flat SS does not encode
the linker payload boundary. An unexpected DF would be FAIL, not a rescue pass.

The ordinary entry saves registers to fixed RAM, reloads DS/ES/FS/GS, sets ESP
to fatal_top at0x001000ea, aligns at0x001000ef, then first CALL0x001000f2. No
old-stack software push/call occurs before that pivot. A later diagnostic ESP
does not retroactively change cpu_esp/origin_esp or turn low headroom into a
frame-validity failure.

## Exact Required Predicate

The future instance needs one actual vector6 at its named UD2, no vector8,
normal TR0x28, correct raw frame/EIP/CS/flags, initialized latch/records and true
other admission conditions. Published primary must have version1, kind1,
vector6, error0, cs8, context_source1, frame_valid1, origin_valid1,
eflags_known1, cr2_applicable0, stack_kind1, cpu_esp=B+1020,
origin_esp=B+1032, unsafe_flags0x04 only and persistence_eligible0.

Require actual runtime/storage readiness, trusted store and valid scan/order,
depth0, correct root/text/map/task state. Generic ineligibility, a locked store,
stale message or a copied magic field cannot establish the intended rejection.
CR2 is an observed register for #UD, not a new fault-address provenance claim.
Saved fault EFLAGS, including RF handling, must follow the pinned CPU/frame
semantics and pre-fault evidence; no post-run arbitrary flag mask is acceptable.

The O terminal is the unsafe branch: reference CLI/HLT/backedge at
0x00102ec0/0x00102ec1/0x00102ec2. The dispatcher-return loop at
0x00102eb0/0x00102eb1/0x00102eb2 has the same instruction pattern and fails O.
HLT1, IF0, unchanged root/task, bounded fatal ESP and stable samples>=250ms
are necessary; PC alone is insufficient. Healthy serial must include the whole
primary numeric block. Primary output can corroborate RAM, not replace it.

## Missing Execution Prerequisites

| Gate | Missing concrete evidence | False positive if omitted |
|---|---|---|
| U-BUILD | Actual default-off pivot/arm/UD2 source, ELF, addresses, no-spill instruction witness, exact O caller/terminal binding. | Wrong trigger or call prologue silently changes stack geometry. |
| U-RAM | Full live-frame nonoverlap, post-pivot RAM baseline domain, exact fixture allocations, all reachable diagnostic stack/store intervals within4096 cap, precise CPU bit masks. | Broad stack/table mask conceals unrelated corruption or pre-keyboard baseline includes expected activity. |
| U-CMOS | Pinned RTC source-to-binary/update-path proof, known overwrite, lossless continuous trace and calibrated same-value/all-index writes. | Silent or incomplete sensor plus disk equality falsely certifies CMOS preservation. |
| U-WRITER | Exact forbidden entry/inline/call-edge registry, simultaneous capacity, dynamic coverage and positive calibration. | A writer entered but returned early, leaving no ATA traffic. |

All four are explicitly open in the revision. It is correct to leave future
numeric bindings unknown before any authorized fixture build. It is incorrect
to promote their absence to ready-for-execution because the semantic design is
improved. A separately bounded A1-I preparation scope and later independent
build/observer review are distinct owner decisions from A1-E.

## RAM And Contamination Conditions

E-live follows arm consumption and CLI on the healthy original stack. Capture
true live-frame extent and prove disjointness from the low target extent.
E-fault follows the pivot immediately before UD2; only twelve source bytes may
then change. No later keyboard confirmation, PIT delivery, fixture spill or
debugger-inserted INT3 may contaminate that epoch. IF0 excludes maskable IRQ
delivery, not every NMI/device/host effect; the exact model and event stream
must account for other possible changes.

The source remainder/old live frames, adjacent reservation and all unrelated
normal RAM remain immutable. Fixed entry/capture/latch and independently proven
diagnostic stack are separate allowances, not a whole-BSS exemption. PDE/PTE/
descriptor CPU-bit changes require precise predeclared words/bits. Debugger/QMP
physical reads must target ordinary RAM, with MMIO/ROM accounted separately.
Endpoint equality does not prove no transient/idempotent write attempt.

The intended source frame does not need adjacent-memory corruption. Whether
the entire handler stays within its future owned memory remains UNVERIFIED
until the specified store-domain and observation proof exists.
