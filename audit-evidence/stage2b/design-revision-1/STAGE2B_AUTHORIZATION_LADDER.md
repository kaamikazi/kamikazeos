# Stage 2B Revision 1 — separate authorization ladder

**This document grants no authority.** It describes what evidence a future owner
decision would need. Current authority is design/read-only inspection only; no
H0 implementation/control run, fixture, build, calibration, test or VM is included.
Independent design review and an explicit owner grant are different events.

## A. Gate meanings

1. **Design review:** assess the semantic mechanism, preconditions, observable
   predicates, finite control catalogue, allowed mutations and unresolved items.
2. **Implementation/build authorization:** a separate written owner decision names
   exact host-only or isolated default-off test-build work, allowed files/allocation,
   one case/checkpoint and permitted build/control preparation. It grants no
   implicit guest execution. U-BUILD cannot be filled without this later authority.
3. **Build/observer review:** independently bind actual instructions/bytes/CFG,
   source/ELF/model/observer hashes, allocations and proof coverage. Source semantics
   alone do not establish a linked hook or a working sensor.
4. **Calibration authorization:** separately name bounded host controls or healthy
   calibration workload, writable copies and model/sensor edges. Actual executions,
   including healthy/panic controls and CMOS writes, require explicit scope.
5. **Case execution authorization:** only after prerequisites/controls are reviewed,
   name one fresh instance, event(s), parameter, deadlines and allowed deltas.
   Passing an earlier gate or case is not a grant to run the next one.
6. **Result/independent acceptance:** preserve and evaluate raw evidence before any
   fresh repeat, next case or regression. A successful author run is not independent
   acceptance; an UNVERIFIED subclaim cannot be silently omitted.

No gate clears a failed record, rewrites a historical result or authorizes kernel
policy changes. If a gate needs source/model changes beyond its named proposal,
stop for a new decision. No phase progression, branch operation, END integration
or production protection is inferred.

## B. Smallest first consideration: H0-Core only

| Gate | Required input / possible separate decision | Exit condition |
| --- | --- | --- |
| H0-D | Review this revision and `STAGE2B_ORACLE_MATRIX.md` catalogue HP01–08/HN01–48, actual reference identities, pure-data/synthetic-versus-retained separation. | Independent review finds a finite deterministic catalogue; unresolved reviews remain visible. This revision is only submitted, not accepted. |
| H0-I | Owner may separately consider **pure host-data implementation** and exact new files. No guest hooks/device model/VM/UART backend or original observer changes. | Isolated code/input bytes/hash manifest prepared under that future authority. No control has run merely because files exist. |
| H0-C | After observer/data review and explicit host-control execution authority, run only the reviewed finite controls and unchanged applicable legacy controls. | Every positive accepted in its correct synthetic/retained mode; all negatives classified as specified, none promoted to new hardware evidence. Preserve every result. |
| H0-R | Independently review actual host-control outputs and preservation; no weakened Stage 2A62+4/healthy serial contract. | Evidence to consider a **new** narrow A1 proposal, not authority for it. |

H0 implementation cannot calibrate a real CMOS model or live breakpoint registry
using data tuples. Device/backend controls remain outside this first host-only
scope. Missing concrete control data/binding at implementation review blocks H0-C;
the written catalogue is not a completed control result.

## C. Next possible guest proposal: A1-UD-1020 only

| Gate | Required evidence / decision |
| --- | --- |
| A1-D | Independent review of the single #UD/S=B+1032 contract, isolated BAD_STACK0x04, valid frame/origin1, healthy trust/depth and M0–M6 epochs/budgets. F02/F04/F05 residual U items remain visible. |
| A1-I | Separate owner scope for one isolated default-off test-build pivot/UD2 and two-DWORD arm/ack, plus distinct O observer work. No production edit or other A1/P1/R1 parameter is implied. Build permission, if any, must be explicit. |
| A1-B | Exact source/ELF/observer identity; UD2 immediately after pivot with no call/push/spill; active-live-frame nonoverlap; correct descriptors/maps/SS; new O CFG/fallback rejection; exact allocations, <=4096 diagnostic stack/store proof, per-word CPU bit masks and W <=16-site coverage/capacity. Close U-BUILD/U-RAM for this build. |
| A1-C | Separately scoped controls/calibration, including new-build O false positives, preserved legacy baseline, same-model IDE/CMOS sensor coverage and CMOS-T1 audited source/binary/cell-update/lossless-trace chain. Close U-CMOS/U-WRITER for this instance. No fault may be used to bootstrap a missing observer. |
| A1-E | Only after independent review of A1-B/C and explicit owner grant: **one** fresh A1-UD-1020 instance,60s boot/30s confirm deadline,32-MiB trace cap, >=250ms stable halt, full CPU/RAM/record/frame/image/reconstructed-CMOS evidence, zero forbidden writer/request. |
| A1-R | Independently inspect author evidence and preservation. FAIL or UNVERIFIED stops. Any fresh repetition or compatibility selection needs its own explicit scope; no automatic retry/continuation. |

At this revision U-BUILD/U-RAM/U-CMOS/U-WRITER are unprovided. A1 is a pinned
**design-review candidate**, not implementation/execution-ready. A future build may
be needed to close bindings, but this document grants no build permission.

Additional zero-headroom #UD and #PF parameters require separate named designs/
owner decisions after review of the initial A1 evidence. They are not adaptive
fallbacks if the first parameter fails.

## D. P1 and R1: independent later decisions

Following reviewed H0 and the ordinary unsafe O profile, the owner could consider
P1-LK-UD and R1-UD-EMIT0 as **separate** proposals. P1's unavailable topology runs
and R2's early-entry runs are not prerequisites for R1. Risk ordering is not a
blanket grant or a requirement to execute unnecessary faults.

### P1-LK-UD

1. Review the one seed recipe, prepared-versus-settled image epochs, real lock/trust
   fields, exact unsafe0x10 result, W no-entry proof and applicable sensor controls.
2. Separately authorize any implementation, **new seed-copy preparation**, build
   and observer work. Never attach/mutate the historical seed itself. Obtain and
   freeze the proposed copy hash, currently UNRESOLVED—NOT CREATED.
3. Independently review actual equal-generation valid/unequal sectors, journal
   entries/ordering, initialized ambiguity tuple and future build/observer registry.
   Verify RAM generation0 versus disk generation5, real depth0/runtime1, no flag
   forgery or auto-repair. Qualify CMOS-T1 and W with separate live calibration.
4. Only then consider a separate one-instance execution decision. Zero ATA events
   alone never suffices; writer API entry fails even if its lock returns early.

P1-NC-UD and P1-ND-UD remain distinct later topology proposals, **U-DEVICE**.
They need exact emulator launch/tree/port behavior and per-profile calibration:
NC's missing device callback is not an OUT-attempt sensor; ND's IDENTIFY events
are not media-write completions. Disk equality is N/A for no attached target;
CMOS/W obligations remain mandatory. No combined unavailable/locked grant.

### R1-UD-EMIT0

1. Review parent #UD at the existing trigger, first primary-emitter/pre-prologue
   C-EMIT0 caller/args, published eligible1/unsafe0/auth0, real store1/depth0,
   complete128/100 snapshots and roomy live fatal stack. H0 R-N controls must pass.
2. Separately authorize implementation/build of only the reviewed one-shot
   C-SEC UD2/arm prefix and distinct R-N observer. Future numeric binding comes
   from its actual ELF, not P offsets or an approximate emitter/source-line hit.
3. Independently review actual no-spill/no-clobber hook and prospective frame,
   exactly[6,6] event/site/raw-frame proof, counter1/sparse auxiliary, immutable
   parent128/entry100, exact reentry terminal and zero parent emission prefix.
   Revalidate per-build RAM/W/CMOS coverage and writer-free window from parent fault.
4. Only then consider one-instance execution. Already-denied parents, later output
   bytes and after-authorization variants require new cases; they cannot pass R1.

**No prior R2 execution is required.** R1's actual completed delivery/publication
and current fatal-stack observations establish its entry state. The review's
source-grounded R-N controls are required; a generic reentry HLT/message is not
the evidence. No primary resume, record rewriting or tertiary injection is allowed.

## E. Later case gates and retained deferrals

| Candidate | Required separately reviewed prerequisite; no present grant |
| --- | --- |
| R2-PRE-ESP-UD / R2-POST-ESP-UD | Reviewed R-N controls/results, two exact partial-state/stack/store contracts and one-case-per-checkpoint scope. Pre-MOV versus postalignment/pre-CALL cannot be one run/range. |
| R3-PRE-MAGIC-UD | Primary destination/copy-complete/cleanup/pre-store witness, complete magic0 pre-image, byte-only semantic limit and its own hook/mask review. |
| P2-READ-CMD-UD | Reviewed ordinary storage/unsafe profiles, owned sector/buffer and exact READ20/count1/words0/depth1 checkpoint, model phase/status evidence, U-P2 closure; expected unsafe0x30. |
| P2 DRQ/transfer/wrapper/backend alternatives | Distinct operation phase, count and observation contracts. No positive-depth-only substitution for CMD. |
| A2 reservation crossing | Prior reviewed A1 and exact four-byte reservation delta/nonownership permission, independent raw-frame/table preservation proof. Guards stay mapped. |
| U1 absent / discard | Separate actual device profiles and serial-independent initialized boot/arm oracle; immutable healthy Stage2A assertions. No null-as-busy or timeout-as-boot shortcut. |
| U2/U3 busy/boundary/partial | Returning deterministic UART model, exact byte/poll counters, coverage/capacity/time review and separate calibration authority; U-UART. No nonreturning host I/O claim. |
| R4 DF reentry | Preserved completed DF evidence, R-D saved/live NT distinction, roomy frame, parent/TSS immutable and genuine[14,8,6] sequence. No DF-before-latch or repeated task-gate delivery. |
| R5 sequential PF | Reviewed R1, two distinct actual addresses/checkpoints, completed first delivery/publication and CPU/primary/auxiliary/trace CR2 provenance. |
| U4 combined UART/reentry | Corresponding individual R/U evidence, one fixed combination, assembly-specific bounds and independent observation; no third fault. |
| P3 WRITE / FLUSH | P2 evidence plus exact model/operation/payload/submission/data/flush/completion/atomicity ledger, immutable unrelated sectors and new explicit delta authority; U-P3. |

Special-SS/#SS, recursion, pre-latch ownership loss, DF hardware-delivery failure,
busy-DF reinvocation, mid-copy/auxiliary publication, third-fault storms, corrupted
architectural metadata, higher-level transaction/CMOS-sequence interruptions and
physical-hardware experiments remain deferred. A later source change or model
confidence is not their missing evidence or permission.

## F. Universal stop and preservation rules

Keep one case per fresh instance and validate before any next case. Unexpected
events/state, unapproved RAM/image changes, writer entry/request, resets/triple
fault, missing calibrated coverage, unresolved binding, runaway output or deadline
stop execution and freeze artifacts. Capture best-effort state, then terminate;
never clear busy/evidence, change checkpoint, expand masks/limits, weaken controls
or overwrite results to get a pass. UNVERIFIED blocks progression like FAIL.

All later work must retain exact original source/images/audits/ref inventories,
accepted Stage1/2A evidence, original failed observation and exit1. Each new
instance/build/calibration has its own directory, identities, before/after images
and report. No automatic reboot/retry or same-instance recovery is authorized.

The current seven-document revision is ready for independent **design** review.
No independent verdict or owner implementation/execution grant has been received.

**REVISION READY FOR INDEPENDENT REVIEW**
