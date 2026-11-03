# Independent adversarial Stage 2B design review

## Decision scope

The hardware reasoning is substantially sound, but the proposed **H0+A1+P1+R1
batch is not sufficiently unambiguous for owner implementation-authorization
consideration as written**. Fix the first-tranche oracles and narrow the named
variants. This is a design revision finding, not a newly demonstrated kernel
failure, and does not overturn the preserved Stage 2A acceptance.

This review ran no builds, tests, QEMU instances, CPU exceptions or fixtures.
Only read-only source/Git/hash/ELF/disassembly and preserved-artifact inspection
was performed. No production, host-observer or original-design file was changed.

## Baseline and evidence legend

- Production: `${REPOSITORY}`, branch
  `feature/v2.6.2-memory-protection-phase-b`, clean HEAD
  `dbf23ce74a2a7103ae6fce04fba51969b7807178`;
  tree `62ff1f4fbd5d70163c074fb7c259f4f2115c4d77`.
- **D** = immutable reviewed design:
  `${LOCAL_EVIDENCE}\kmz-v262-stage2b-design-dbf23ce\STAGE2B_DESIGN.md`,
  SHA-256 `ae77ff66804a2ed98e8d2f0cab8b1ed15722749872c3ece782c7f72869a5c9fe`.
- **K** = production `kernel/` directory. Source references below are relative to K.
- **P** = production ELF, SHA-256
  `7cc1262fb1caee2b104d17d601a19e172eb02eac6d93b56ad2b2612a228db7bc`.
- **A** = preserved independent Stage 2A tree:
  `${LOCAL_EVIDENCE}\kmz-v262-stage2a-acceptance-dbf23ce`;
  executed ELF SHA-256
  `79415990686f9ab0579c6f86f216e1fd7d372965eb7e7bf2f0e7e952ffecdd50`.
- Full newly generated read-only disassembly: `PRODUCTION_DISASSEMBLY.txt` and
  `STAGE2A_ACCEPTED_DISASSEMBLY.txt` in this review directory. Addresses cited
  without an A qualifier refer to P, not a future instrumented build.

### Demonstrated earlier, inspected now, and not demonstrated

| Claim | Assessment in this review |
| --- | --- |
| Production commit/tree/clean state and listed ELF identities | Independently checked by Git/content hash inspection; see provenance JSON |
| Earlier Stage 2A independently accepted | Existing `A/acceptance-verdict.json` and report present; raw entry/terminal RAM, CPU, trace and image captures corroborate the stated mechanism |
| Earlier genuine PF-during-PF, TR28->30, DF stack/error/busy/backlink | Corroborated from preserved artifacts, **not newly executed here**; receipt `EXISTING_STAGE2A_EVIDENCE_INSPECTION.json` |
| Earlier 66 compatibility results and two case exits 0 | Recorded summaries/results are available and hashed; no test was rerun here |
| Original failed observation remains failed | Original `case1/result.json` remains `passed=false`, exit 1; SHA-256 `70600df8e31af2bfc35235bc03310c913160fd58c5ef59cc39ef68abcca19a1a` |
| 62 original assertions retained plus four additions | Static AST/source inspection finds 62 versus 66 `need` expressions, none removed, no early return; not a new runtime assertion test |
| Author ran no Stage 2B experiments | Design/audit/status artifacts are consistent with this; no Stage 2B fixture or run evidence supplied. Absence of every possible unlogged historical action cannot be independently proved; that stronger claim is UNVERIFIED |
| New Stage 2B behavior | UNVERIFIED; no experiments authorized or performed |

## Findings requiring revision

### F01 — Medium, first-tranche blocker: A1's validity/failure predicate is ambiguous

**Evidence:** D:331-339 first requires a structurally valid primary and then says
"A DF, valid/eligible classification ... fails." `src/emergency_frame.c:35-55`
sets frame_valid/origin_valid for a complete legal frame; `src/emergency_record.c:12-17`
separately rejects insufficient source headroom. Those are different properties.

For a healthy otherwise-eligible A1 #UD case, the correct target is a published
record with **frame_valid=1, origin_valid=1, EM_BAD_STACK set, eligible=0**. A literal
"valid classification fails" oracle rejects the correct result; interpreting the
slash differently yields a different oracle. This is not an arithmetic flaw in
the proposed ESP positions, but it is not an acceptable normative PASS/FAIL rule.

**Required revision:** state exact field predicates. Make all other admission
conditions true and independently observed so the first minimal case can require
the isolated rejection reason `unsafe_flags=EM_BAD_STACK`, not hide behind an
unavailable store, wrong CR3 or undecoded frame. Never treat record publication,
frame validity, origin validity and persistence eligibility as synonyms.

### F02 — Medium, first-tranche blocker: RAM-write scope and comparison boundary need a normative partition

**Evidence:** D:323-329 describes only the hardware frame in the lower source
payload, but D:337-338 says "any write outside the approved frame fails." Actual
normal handling necessarily writes `emergency_entry_state` (100 bytes), a primary
record (128 bytes), the latch, and its diagnostic-stack frames. See
`src/emergency_entry.S:18-47,59-61`, `src/emergency.c:90-111`; machine ESP move at
0x1000ea and C call at 0x1000f2. R3's "no writes" must likewise mean persistence,
not the required auxiliary record/counter/frame writes.

The wording may intend only source-stack writes; it does not say that precisely
enough. Comparing all RAM to a baseline before keyboard confirmation also includes
legitimate input/PIT/fixture activity before CLI. A broad after-the-fact allowance
would instead hide corruption.

**Required revision:** distinguish (a) source-frame allowed interval, (b) normal
handler-owned records and bounded diagnostic stack activity, (c) fixture-owned
arm/ack RAM, (d) CPU-only A/D/accessed/task effects, and (e) immutable other RAM.
Capture the strict A1 delta baseline at the post-pivot/pre-fault instruction
boundary with IF clear, in addition to the original-live-stack snapshot. Record
all masks before execution. Do not interpret this as allowing arbitrary writes
inside all kernel RAM or changing a mask after a failed result.

### F03 — Medium, R1 authorization blocker: one publication/authorization variant must be pinned

**Evidence:** D:364-368 permits an ordinary parent and a "proved emitter/sink
checkpoint" while saying eligibility "may" be 1. `src/emergency.c:108-110` and
P:0x102e58..0x102e8a show capture, emission, then the ordinary-authorized store.
The generic emitter is also used by DF. These locations/states are not interchangeable.

R1 can test byte preservation with an already-ineligible parent, but that does not
show diversion of an otherwise permitted panic-persistence path. A breakpoint
somewhere in a generic sink does not alone establish which record/caller/phase is
being interrupted. This is an avoidable false-positive mechanism claim.

**Required revision:** choose one named parent vector/site and one symbolic
checkpoint (for example the first primary-emitter entry from ordinary handling,
after publication and before authorization). Require primary destination identity,
magic/fields, eligibility=1, storage_ready=1, no I/O, ordinary_authorized=0 and the
live fatal-stack extent before the single secondary #UD. Describe any ineligible
parent or later-byte variant as a separate case, not an alternative successful R1.
Require positive secondary CPU vector/site/raw-frame evidence, counter delta=1
and unchanged 128/100-byte parent snapshots. H0 must reject a debugger-only stop,
the unauthorized-formatter shortcut, and a generic emitter called for the wrong
record. The first-tranche decision explains why R2 is not a prerequisite.

### F04 — Medium, persistence-oracle blocker: recovery CMOS equality lacks an operational measurement chain

**Evidence:** D:334-335 and 585-601 require recovery-CMOS equality as well as no
new writes. `src/recovery_memory.c:75-114` issues nine data writes through I/O
ports, not ordinary RAM. QMP `pmemsave` captures used in A do not read these cells.
Stage 2A's retained I/O trace is good evidence of no recorded requests, but there
is no supplied direct pre/post recovery-CMOS snapshot mechanism for the new cases.

**Required revision:** choose and justify a bounded, non-mutating measurement:
either a verified emulator state export, or explicitly reconstructed recovery
cells from complete known writes plus a continuous independently calibrated
trace and a pinned RTC model that does not autonomously alter 0x60..0x68. Do not
invent a QOM property or debugger-call guest CMOS code. An index-select/read
scheme must address port-0x70/NMI and read-side effects before consideration.
Keep clock/status evolution separate. Until this is specified, equality of the
recovery block is UNVERIFIED, not inferable from a disk hash.

### F05 — Medium, P1 authorization blocker: unavailable-device success must not be vacuous

**Evidence:** D:343-359 wants real absence/locks and treats a writer call as
failure. Yet `src/recovery_store.c:483-486` and `577-584` can return before any
ATA request when unavailable/locked; `src/ata.c:246-263` also rejects invalid
requests before commands. Thus zero ATA events alone does not prove a forbidden
writer edge was never entered. Conversely `recovery_memory_save` always writes
CMOS before its conditional disk save, so CMOS monitoring is indispensable.

D:231-235's generic positive IDE/CMOS-control requirement also needs a per-profile
applicability rule: no-controller/no-device variants cannot be presumed to produce
healthy media-write calibration events during that same boot.

**Required revision:** split no-controller/no-disk/locked-image variants; freeze
one seed recipe and expected initialized lock/trust fields. Add a static forbidden-
call-edge proof and/or bounded independent entry-site observation that fails on
writer entry even if lower guards suppress physical I/O. Supply a healthy
calibration reference for the applicable sensors, without creating artificial
writes in the unsafe case. Pin the no-new-request fault boundary and qualified
CMOS oracle. This is a causal test-design gap, not a claim that the current
unsafe branch calls these writers.

### F06 — Medium for later authorization: range descriptions are not yet one experiment

**Evidence:** R2 (D:383-390) spans multiple instructions with different partial
entry-state contents; P2 (D:434-445) leaves an after-command/before-transfer-or-
completion interval; P3 leaves distinct data/flush phases conditional on a later
ledger. A software depth value can remain positive after media/backend completion
while transfer/API completion is still pending.

**Required revision:** bind each variant to a single compiled semantic boundary
and expected state. For R2, select one pre-MOV-ESP point and one post-alignment/
pre-C-call point. For P2, distinguish actual PIO operation state from merely
pending wrapper return; a post-command/pre-first-INW point with command/LBA,
transfer count and controller status is a falsifiable option. Do not demand an
uncompleted host disk read when the intended invariant is software PIO activity.
P3's exact completion/delta ledger remains correctly deferred; no retrospective
old/full/partial allowance is acceptable.

## Adversarial checks that did not establish a defect

### A1 hardware arithmetic is correct under the declared baseline

Assume 32-bit protected mode, CPL0, flat 32-bit SS, VM86/CET absent, correct 386
interrupt gates and the preflighted accessible RAM. For initial ESP S, the frame
occupies [S-F,S), no saved SS/ESP tail at the same CPL:

| Case | S | Final CPU ESP | Frame interval | Source headroom |
| --- | --- | --- | --- | --- |
| #UD F=12 | B+12 | B | [B,B+12) | 0 |
| #PF F=16 | B+16 | B | [B,B+16) | 0 |
| #UD F=12 | B+1032 | B+1020 | [B+1020,B+1032) | 1020 |
| #PF F=16 | B+1036 | B+1020 | [B+1020,B+1036) | 1020 |

P's B=0x14e000 and T=0x152000, with adjacent mapped reservation
0x14d000..0x14e000 and tables ending 0x14d000. These frames remain inside the
16-KiB payload; no reservation/table corruption is necessary. #PF error code 0
is appropriate only for the specified supervisor read of an actually nonpresent
page; verify raw tables and the faulting instruction.

The common assembly has no old-stack software push/call before moving ESP, so
those fixed first frames need not escalate. A subsequent delivery/access fault
still could; unexpected DF is failure, not success through a rescue path. The
synthetic historical PUSHA ESP (`CPU ESP-8` or `-4`) is not a followed pointer;
it can lie below B without an extra memory write. A1 demonstrates policy-headroom
denial, not recursive overflow or failure to accept a hardware frame.

### Task ownership and incomplete evidence are mostly handled correctly

`src/x86_task.c:24-42` prepares the DPL0 task gate and known task state.
`src/double_fault.c:25-41,52-58` establishes prerequisites and verifies TR/backlink
before reading previous TSS. Existing A captures corroborate TR30, error word 0,
busy 0x8b/backlink28 and NT=1 under completed uninterrupted delivery. This review
did not rerun that experiment.

The pre-latch window is real: P reads CR2 at 0x1001d2 before BTS at 0x1001ea.
Deferral is appropriate. Reentry assembly writes auxiliary data and never resets
ESP; P's terminal loop is 0x1001c5/0x1001c6/0x1001c7. It can still fault on its
hardware frame, DS-based auxiliary stores, literal load or I/O if prerequisites
are violated. No C calls does not mean architecturally infallible.

The DF record has mixed field provenance. `src/emergency.c:125-132` takes CR0/CR2/
CR3 from the new task's early observed registers, but EIP/CS/EFLAGS/ESP from the
verified previous TSS. Do not read previous-TSS CR3 as proof of the earlier CPU
root or label current CR2 as the initiating read address. Current pre/post CR3
is the same known directory; preserved Stage 2A does not demonstrate rescue from
a corrupted/different original root. This limitation is not a request to corrupt
CR3 or page tables in Stage 2B.

R3's semantic boundary is sound: P calls memcpy at 0x1037b6, then publishes magic
at 0x1037be. Destination qualification is mandatory. A magic-zero copied body
is not published evidence. R4's live/saved NT distinction is consistent with
interrupt-gate semantics; it is an expected future outcome, not observed Stage 2B
behavior. A new ordinary exception after DF delivery completes is not necessarily
DF/triple fault; failure during DF delivery or a required second DF into its busy
TSS can triple fault. Deferring those cases is correct.

### UART and persistence analysis has important correct boundaries

`src/serial.c:43-50` and P:0x11ca92..0x11cada prove a qualified presence check,
100000 clamp and decrement/backedge. Emergency C requests 4096; assembly reentry
uses a separate <=64-byte/4096-poll loop without that presence check. D correctly
distinguishes absent, discard, busy, final-poll, partial and nonreturning-backend
models. Its 8,388,608 conservative physical-poll upper bound is safe for a single
minimal emission; permanent first-byte busy stops after <=4096. Finite polling
does not bound a host I/O operation that never returns. U2/U3 deferral is proper.

Both panic formatters eventually use the guarded `record_panic` edge
(`src/panic.c:56-61,74-84,112-129,185`). Mark-panic calls RecoveryMemory save:
CMOS 0x60..0x68 then trusted-order conditional state save; journal append writes
its sector and saves state again. State save writes mirror50 then primary1.
`src/ata.c:250-263` covers actual attempts/retries, including write/flush work.
It does not cover whole metadata transactions or a CMOS sequence. D correctly
refuses a universal barrier claim and separates already-submitted completion
from a new handler request. Later P3 must make that ledger concrete.

### Oracle principles are good but not yet fully operational for the first tranche

Exact-ELF CLI/HLT/backedge derivation, HLT/CPL/TR/CR3/selectors/stack checks,
>=250-ms stability, raw guest-exception evidence, no automatic return, independent
reset/trace detection and immutable failures are all appropriate. Two source
shortcuts illustrate why the additional checks matter:

- P:0x102eb2 is a dispatcher-return fallback, not profile O success at 0x102ec2.
- `panic.c:76,115` can jump directly to reentry halt without a genuine secondary
  event/counter/auxiliary capture. A fixed reentry message or PC is insufficient.

H0 must specify these positive/negative state tuples rather than borrow the DF
observer and relax its task/serial requirements. An interrupted parent output
prefix is legitimate for reentry, but a full healthy Stage 2A numeric block must
not become optional in the original baseline.

## Required dispositions

The complete per-case nine-part assessment is in `STAGE2B_CASE_MATRIX.md`.
The proposed first batch should be split, as explained in
`STAGE2B_FIRST_TRANCHE_DECISION.md`. The main x86 mechanisms are not rejected;
the current normative oracles and variant selection must be repaired first.

The separate future-architecture review finds no reason to introduce mandatory
virtualization or AI dependence. It identifies actual current single-CPU/static-
layout/user-fault limitations as future work, not Stage 2B feature requirements.

## Evidence and non-execution limits

All paths inspected were available. Existing Stage 2A captures substantiate its
accepted narrow result, not generalized overflow/reentry/UART safety. Current
Stage 2B case outcomes remain UNVERIFIED. This review produced no runtime result
and changed no original design, kernel, host observer or historical artifact.
Preservation details are in `STAGE2B_PRESERVATION_REPORT.md` and provenance JSON.

REVISE BEFORE AUTHORIZATION
