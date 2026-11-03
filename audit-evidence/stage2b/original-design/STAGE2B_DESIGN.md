# KamikazeOS v2.6.2 Phase B — Stage 2B design and review proposal

## 0. Status and authority

**DESIGN ONLY. Every new Stage 2B experiment below is UNVERIFIED and NOT RUN.**
This document is a proposal for review, not authorization to build fixtures or
execute them. This run performed file/hash/Git inspection and static disassembly
only: zero builds, zero QEMU launches, zero tests and zero fault fixtures.

Repository: `${REPOSITORY}`; branch:
`feature/v2.6.2-memory-protection-phase-b`.
Production commit: `dbf23ce74a2a7103ae6fce04fba51969b7807178`.
Tree: `62ff1f4fbd5d70163c074fb7c259f4f2115c4d77`. Initial tree was clean.

Accepted inputs read: `docs/MEMORY_PROTECTION_PHASE_B.md`, Stage 1 report,
independent Stage 2A acceptance report, relevant source and compiled paths. The
Stage 2A baseline is the accepted fixed #PF-during-#PF task-gate experiment and
its exact-ELF observer, 62 original assertions plus four added guards, negative
controls, 60-second boot/30-second post-trigger deadlines and storage oracles.

### Preserved binary identities

| Artifact | SHA-256 |
| --- | --- |
| Production ELF | `7cc1262fb1caee2b104d17d601a19e172eb02eac6d93b56ad2b2612a228db7bc` |
| Stage 1 ELF | `cb282795cff5639a0cec8608a38cb0c01e18a72801264698aba836b0692bd5ee` |
| Original Stage 2A fixture | `ae7d6975f5156581fc1af0f346f7f4e1b69062ba49971af371dd80d5861e9e66` |
| Corrected revalidation ELF | `55697ce3bd4336a201da7ae93f3cabbe714e142abb84e4c3bd697e0d367a4133` |
| Independently accepted Stage 2A ELF | `79415990686f9ab0579c6f86f216e1fd7d372965eb7e7bf2f0e7e952ffecdd50` |

Copies and full source/historical inventories are retained in this design tree.
Numbers below are observations about those binaries, not future fixture offsets.
Every later approved build must derive addresses and CFG witnesses again.

## 1. Architecture and source-grounded risk analysis

### 1.1 Entry and evidence ownership

`interrupt_stubs.S:8-25` and `emergency_entry.S:65-80` claim a first-entry latch
with atomic BTS. If it was already set, the assembly increments a reentry counter
and takes a separate secondary-record/output/halt path. It does not run the
ordinary C capture/formatter again. Common fatal entry saves GPRs/control state
to fixed RAM and changes ESP before calling C (`emergency_entry.S:16-62`).

The CPU must first deliver an interrupt-gate frame. At CPL0, ordinary #UD/#DE
need 12 bytes, #PF adds a four-byte error code. TSS.esp0 is not used for same-CPL
delivery. Stage 2A established one independent task-gate rescue when #PF delivery
itself encounters #PF; that does not make arbitrary stack corruption safe.

The software common entry changes DS/ES/FS/GS but **does not reload SS**. Any
proposed special-SS experiment must prove that both old-stack delivery and the
new diagnostic payload are legal under the inherited SS until a task switch.

`em_record_capture` copies a 128-byte record, uses a compiler publication barrier,
and stores magic last (`emergency_record.c:40-58`). Published records refuse
replacement. A fault before publication can leave magic zero and partially
written bytes; those bytes are not a semantically complete crash record. Reentry
can preserve the bytes that exist without recovering information never captured.

Secondary assembly records contain vector and observed control registers, not a
complete original CPU frame/GPR set. Its check-before-publication is not a
general atomic first-secondary protocol under a third fault. Propose exactly
one secondary injection per instance; defer tertiary/repeated fault storms.

### 1.2 Stack classification and inactive reservations

`emergency_frame.c:5-15,28-55` follows CPU-frame words only after complete bounds
checks in a named stack payload. `emergency_record.c:12-17` requires boot/privilege
source kind and >=1,024 bytes of source headroom for persistence. Diagnostic/DF
source kinds do not authorize ordinary persistence.

Production bootstrap payload is `0014e000..00152000`, fatal payload
`0015a000..0015e000`, DF payload `00160000..00164000`; each is 16 KiB. All ten
4-KiB reservations and NULL remain mapped. The bootstrap lower reservation is
immediately above page tables. Naive recursion can overwrite mapped reservations
and then live metadata before any fault: **it is excluded, not a Stage 2B test**.

### 1.3 Reentry and DF are distinct routes

Reentry output (`emergency_entry.S:83-131`) makes no C call and never switches
ESP. The secondary exception must still fit a hardware frame on the current
stack. Successful reentry on a healthy diagnostic stack says nothing about a
second exception whose hardware push cannot complete.

DF entry reads control registers before its BTS (`emergency_entry.S:137-146`).
If the initiating frame never reached an ISR, latch=0 in that initial interval.
A fault there could become the new primary owner on a DF source stack rather
than taking the expected secondary route. **Early DF-before-latch injections are
deferred**, not assumed protected by the later reentry guard.

After a task-gate delivery has completed, an ordinary exception inside DF C code
is not automatically a triple fault. With a valid current DF stack it can enter
its interrupt gate and the busy-latch reentry route. Conversely, an exception
that prevents DF hardware delivery/error-code push can triple fault. If another
DF is required after the DF TSS is already busy, the same task gate cannot simply
be used again; descriptor/delivery failures can lead to triple fault.

**NT distinction:** the accepted uninterrupted DF terminal route retains NT=1.
A subsequently completed interrupt-gate exception clears live NT; its raw saved
secondary frame can retain the pre-delivery NT=1. A DF-task reentry profile must
therefore expect TR=0x30 with live NT=0, while the saved secondary frame has NT=1.
This is a distinct profile, not a weakening of Stage 2A's NT=1 requirement. No
IRET, task return, busy-bit reset or recovery into the abandoned handler is allowed.

### 1.4 Authorization is control-flow-dependent

`emergency.c:45-71,90-120` requires a ready, trusted store, no active ATA, a valid
source/context and eligible first record before normal formatting/persistence.
`ordinary_authorized` is set only after the minimal numeric emitter returns.
Thus a secondary fault during that first emission must divert before ordinary
persistence even when the first record correctly says its original context was
eligible. **Do not rewrite that first record's eligibility to manufacture safety.**

Reentry does not globally revoke every already-latched authorization variable.
Its actual assembly route has no writer calls and never resumes the primary
path. The claim to test is no new writes from that route, not that arbitrary
out-of-band calls to lower-level persistence APIs are magically prevented.

### 1.5 UART behavior differs by route

- Production C `serial_putc_bounded` checks a boot-latched presence flag, clamps
  caller budget to 100,000 polls and returns failure after its finite loop.
- Emergency C `console_put` uses 4,096 polls per physical byte; LF expands to CR
  plus LF. The record emitter caps logical output at 1,024 characters and makes
  no further sink calls after failure. Its remaining formatting work is finite.
- Permanent THRE=0 starting at the first minimal-emitter byte causes at most
  4,096 status reads before sink failure. A conservative whole-emission bound,
  allowing two physical bytes per logical character, is 2,048 * 4,096 = 8,388,608
  status reads; actual fixed-format output is smaller. These are path-local
  bounds, not a global guarantee under unlimited external faults.
- Reentry assembly ignores the C presence flag. It attempts <=64 literal bytes,
  <=4,096 polls each: <=262,144 status reads. Permanent busy fails on the first
  byte after <=4,096 polls. A floating absent bus returning 0xff has THRE set;
  the assembly may output the whole bounded message even though no device exists.
- `-serial null` is a present/discarded-output model, not proof of absent UART or
  stuck THRE. `-serial none` must be checked as actually removing the device.
- Finite polling cannot bound a backend that never returns from an I/O operation.
  Any busy-device model must return each IN/OUT promptly; truly blocking host I/O
  is not an acceptable CPU-fixture model.
- UART failure alone does not make an otherwise eligible ordinary panic unsafe.
  It may still intentionally persist. First live UART cases should use an already
  ineligible path (DF or low-headroom) so output and authorized storage do not mix.

## 2. Actual compiled observations

Full static disassembly was saved, including local labels; selecting only global
function names can omit selector/poll/halt blocks and is insufficient here.

| Production ELF observation | Design consequence |
| --- | --- |
| Common ESP replacement at `001000ea`, alignment at `001000ef`, first C call at `001000f2` | Before/after-switch cases must use separate instruction-boundary checkpoints; no hypothetical half-written ESP |
| Reentry CLI/HLT/backedge at `001001c5..001001c9`, post-HLT `001001c7` | New reentry observer must validate this assembly CFG and context, not accept any HLT |
| Unsafe ordinary terminal post-HLT `00102ec2`; dispatcher-return fallback post-HLT `00102eb2` | The fallback is NOT success for a low-headroom case; require the unauthorized/unsafe branch and eligibility=0 |
| DF C terminal post-HLT `001030d2` in production; `00103172` in accepted instrumented ELF | Addresses are exact-build-specific; preserve the Stage 2A inline-aware derivation |
| `em_record_capture` memcpy call at `001037b6`; publication store at `001037be` | A pre-publication checkpoint must identify the destination and completed stores in the actual future build |
| Serial presence load at `0011ca92` reads `00144bdc`; polling loop has decrement/backedge | Resolve driver-local variables by DWARF or decoded data flow, not an unqualified `present` symbol shared by multiple C files |
| ATA depth increment precedes calls/retries; decrement follows both attempts | Use true operation checkpoints inside the covered interval; do not forge depth flags |
| Panic formatters contain inlined gate/write logic; `record_panic` need not remain a callable symbol | Audit both call sites and inlining; do not set a breakpoint by a nonexistent source helper name |

These offsets are documentation of inspected bytes only. No patch, breakpoint,
device change or execution was performed in this design run.

## 3. Common experiment contract — applies to EVERY future case

### 3.1 Prerequisites and immutable baseline

Before implementation or execution, separately authorize a named case, exact
variant/checkpoint, permitted guest/host/backend delta and expected write set.
One virtual CPU, fixed QEMU/CPU/firmware versions and deterministic configuration
are mandatory. Pin source/archive/ELF/build-tool hashes, model/backend hashes,
observer hashes, complete launch arguments and independent negative controls.

For each future build, independently decode actual machine code, including local
labels, validate the selected injection point and every permitted terminal loop,
and prove no unintended return, indirect transfer or handler recursion. Do not
use source-line breakpoints as instruction-accurate boundaries.

Preserve the Stage 2A test/observer and its 62+4 assertions unchanged. New normal,
reentry and UART profiles receive distinct test IDs/observers and independent
review; a UART-negative profile is not a Stage 2A PASS with a serial assertion
deleted. Rerun healthy Stage 2A controls under later explicit authorization for
each variant. Add negative controls for wrong PC/task/stack/flags/provenance,
nonhalted execution, resets, invalid publication, missing I/O trace and unintended
serial dependence. None was run here.

At entry, verify live GDTR/IDTR/TR, exact descriptor/TSS fields, CR3/PDE/PTEs,
stack bounds/backing and metadata writability; five USER leaves, ten inactive
guards, NULL mapped, WP clear, CR4=0 and Ring 3 held. Require selected source and
secondary-frame extents wholly in the preapproved region. Validate fixture arm
zero initially, explicit arm/confirm and single consumption. Do not invoke guest
C status functions from the debugger or set registers to fake task delivery.

Verify the selected parent/secondary IDT entries as present DPL0 32-bit interrupt
gates targeting their linked ISR labels (normally type/access 0x8e), as well as
the exact vector-8 task gate. Wrong gate type/target invalidates frame-size and NT
assumptions and blocks triggering; do not merely trust a status-report flag.

Initial clean-instance crash records/counters must be zero. Later parent and
secondary checkpoints require their declared latch/publication state; never
reset evidence to make a preflight pass. Storage-unavailable profiles deliberately
vary storage trust while keeping architectural readiness valid, not vice versa.

### 3.2 Readiness without serial

Healthy profiles retain serial boot checks. For UART-negative profiles, design a
separate CPU/RAM rendezvous at the exact linked `shell_run` entry: `kernel.c:295-308`
marks clean, appends CLEAN_BOOT, attempts the health snapshot, then calls shell.
Prove required initialization and I/O completion by actual RAM/device/CPU state,
not merely by reaching a symbol. Require ATA depth zero and settled boot writes.
Never substitute a 60-second serial timeout for a successful absent-UART boot.

Host QMP and debugger observation remain independent of COM1. Arm acknowledgement
and trigger entry must be observed in fixture RAM/CPU state, not require serial
success. Such observer additions and any test-only rendezvous need separate
authorization; they are not implemented here.

### 3.3 Observation and time limits

Keep 60 seconds to the verified boot rendezvous and 30 seconds from confirm to
terminal proof, including planned debugger stops. Use a host monotonic deadline,
bounded QMP/debugger requests and 32-MiB trace cap. If a reviewed model cannot
finish under those limits, defer it; do not silently increase limits to get PASS.

Retain QMP requests/responses/events, GDB packets, CPU/reset trace, serial capture,
I/O event stream/counters, pre-trigger and per-checkpoint CPU/RAM, descriptor/TSS
bytes, complete disk copies/hashes and qualified recovery CMOS evidence. Validate
trace configuration AND positive sensor activity before interpreting zero events.
Aggregate high-volume UART poll counters with bounded first/last events; do not
allow log volume or dropped events to destroy the oracle.

Pin the accepted combined CPU/I/O trace routing explicitly. The preserved Stage 1
failure showed that this QEMU log-trace backend can redirect CPU output into the
file selected by `-trace`; do not assume `-D` names a separate populated stream.
Require nonempty CPU and positive IDE/CMOS sensor controls in the actual selected
stream, preserve its pre-trigger prefix and use immutable event-window offsets.

Host Z1 execution breakpoints observe; they do not prove a guest CPU exception.
A proposed secondary exception must be a real, single, explicitly selected test-
build event with CPU trace i=0 and a known faulting instruction. No live INT3 code
patching, arbitrary metadata corruption, debugger signals as a fake guest fault,
free-running injector, third fault or automatic retrigger is permitted.

Any later approved one-shot hook may write only its declared fixture-owned arm/
acknowledgement RAM. It must not set production crash magic, change first records,
forge readiness/I/O flags or clobber context it claims to preserve. Sensitive
pre-switch hooks may not introduce C calls, stack spills or old-stack pushes;
consume activation before an intentionally constrained pivot. Prove these
effects from the exact instrumented machine code before authorizing a run.

### 3.4 Terminal and reset profiles

| Profile | Required task/stack/flags | Required evidence and terminal route |
| --- | --- | --- |
| O: unsafe ordinary entry | TR=28, CS=08, SS/data=10, fatal ESP, IF=0, NT=0 | Published ineligible primary; exact unsafe `emergency_handle_entry` halt, not dispatcher-return fallback |
| R-N: reentry from normal task | TR=28, current stack predicted by checkpoint; IF=0, NT=0 | One secondary hardware frame, counter=1, auxiliary kind 2/vector, unchanged parent bytes, exact stackless reentry halt |
| D: uninterrupted DF reference | TR=30, DF ESP, IF=0, NT=1, expected hardware TS | Unchanged Stage 2A entry/TSS/record/halt controls; separate DF record, eligible=0 |
| R-D: reentry after DF delivery completed | TR=30, DF ESP; saved secondary flags NT=1, live terminal NT=0, IF=0 | Published DF record unchanged, backlink/busy retained, secondary vector/frame/counter, exact reentry halt; never interpreted as profile D |

Every profile requires HLT=1, correct CPL/selectors/CR3 and exact-build validated
PC, stable PC/ESP in two observations >=250 ms apart, expected parent/auxiliary
publication/provenance, and the planned task identity throughout. Reentry terminal
ESP is the actual secondary hardware-frame ESP; it is not reset to stack top.
Check raw CPU frame size 12 bytes for #UD or 16 for #PF. No IRET/task return or
resumption of the abandoned primary path is success.

Serial expectations are profile-specific: O requires its primary numeric block
and no normal formatter; R-N/R-D require the fixed reentry literal when UART is
healthy, with any interrupted parent prefix bounded by the chosen checkpoint.
Pre-output reentry need not have a parent numeric block at all. The assembly does
not print a numeric auxiliary record: read that record from RAM. An intentionally
interrupted parent serial block is not permission to alter the unchanged healthy
Stage 2A serial assertions. UART-negative profiles require the separately approved
RAM/CPU/I/O oracle instead of treating silence itself as success.

Use `-no-reboot -no-shutdown` and record reset/boot identity before trigger.
Any new RESET, guest-reset shutdown, triple-fault/reset trace, firmware-vector
execution, unintended vector/task change or process exit fails. Planned host
STOP/RESUME is distinct; host termination happens only after evidence capture or
deadline. A shutdown/paused CPU is not an emergency HLT. At failure/deadline,
capture best-effort state, terminate, preserve everything and stop the sequence.

### 3.5 PASS / FAIL / UNVERIFIED

- **PASS:** exactly the approved event sequence, CPU/task/stack/record and I/O
  outcomes, independent terminal proof, all preservation checks and deadlines.
- **FAIL:** observed violation, including unexpected DF fallback in an ordinary
  case, writer activity outside its ledger, wrong/replaced evidence, task return,
  reset/triple fault, runaway output, timeout or unplanned continuation.
- **UNVERIFIED:** prerequisite not established, backend behavior unproved, missing/
  dropped/ambiguous observation or unavailable initial evidence. This never counts
  as PASS and blocks further execution just like FAIL pending review. A deliberately
  unpublished parent may remain semantically UNVERIFIED while its byte-preservation
  subtest passes; never promote its fields to a valid published record.

Run one case, validate all evidence before any next case, then repeat independently
in a fresh instance only if authorized and all controls pass. No same-instance
recovery, automatic reboot, changed assertions or retries that overwrite failure
evidence. Later compatibility remains a separate gated authorization.

## 4. Proposed cases, ordered by risk

All cases inherit every applicable requirement in section 3 and the storage
contract in section 5. This ranking is not permission to execute. A case marked
CONDITIONAL or DEFERRED has an additional explicit gate before implementation.

### Tier 0 — H0: observer/data/model controls (host-only proposal)

Entry: exact future ELF/route witness and immutable Stage 2A reference. Proposal:
extend pure host controls for O/R-N/R-D provenance, stack/frame boundary arithmetic,
parent magic=0 versus published, serial-independent readiness and bounded-device
event counters. Require rejection of the ordinary dispatcher-return HLT and
unauthorized-formatter shortcut to reentry halt. Retain all Stage 2A controls.
PASS requires every intended negative rejected and healthy reference accepted;
FAIL is a false positive/negative; unresolved route/model semantics is UNVERIFIED.
No guest execution belongs to H0. Even H0 implementation/testing needs approval.

### Tier 1 — A1: hardware frame fits, original headroom is insufficient

Entry: normal ready/TR28, valid flat SS, mapped bootstrap payload [B,T), adequate
unmodified fatal/DF stacks; no active I/O. Use one fixed #UD (F=12) or the accepted
known-unmapped-read #PF (F=16), each in its own proposed instance. Set pre-fault
ESP to **B+F** or **B+F+1020**; after hardware delivery CPU ESP is B or B+1020.
Only that bounded hardware frame can write the unused lower payload. No calls
after pivot, recursion or live page-table change.

Before the pivot, require the normal live ESP at least B+4096 and verify the old
active frame extent/DWARF stack convention; the new <=1040-byte-low target region
must not overlap any live caller frame or other owner. If that nonoverlap proof
cannot be made for the selected build/checkpoint, reject the fixture before use.

Expected: exactly the intended ordinary vector, correct raw frame/error/CR2,
TR28 and profile O. The primary is structurally valid but has EM_BAD_STACK and
eligible=0 because source headroom <1024. C capture/terminal ESP is fatal payload;
no ordinary formatter/persistence, no DF/reentry. Whole image and recovery CMOS
unchanged after the fault. UART remains healthy in this case.

PASS requires these exact outcomes. A DF, valid/eligible classification or any
write outside the approved frame fails. Unproved unused source extent or frame
size is UNVERIFIED. This tests fixed capacity/headroom, not real recursive overflow.

### Tier 1 — P1: unavailable/untrusted storage cannot fall back to panic CMOS writes

Entry: healthy source stack and DF architecture; select either no ATA device or
an independently established read-only ambiguous RecoveryStore image. Use existing
accepted seed/validation rules; no new disk format. Boot normally, verify the real
unavailable/locked state and settle/record boot writes before one ordinary #UD.
Do not forge private readiness/lock flags with debugger writes.

Require runtime/DF readiness true in both variants. For the unavailable variant,
prove actual ATA absence/ata_available=0 after IDENTIFY finishes; for the locked
variant, independently establish state_locked or journal_locked and storage_ready=0
through qualified memory/data-flow inspection of the real initialized image.

Expected: profile O, frame/context valid, EM_BAD_STORAGE/noneligibility, no normal
formatter and zero new ATA/CMOS requests. With no disk, verify no device attached;
with a locked image, require complete post-trigger equality. CMOS-only fallback
messages during boot are not permission for panic writes after an unsafe fault.
PASS requires real lock/unavailability and no new writers. A writer call fails;
ambiguous store state or uncorrelated boot activity is UNVERIFIED. Early boot
before architectural setup is not this test and is deferred separately.

### Tier 2 — R1: one secondary #UD after first publication, during initial output

Entry: an ordinary parent fault has acquired the latch and published its primary
record; stop at a proved `em_record_emit`/sink checkpoint before ordinary
authorization/persistence. Capture all 128 primary bytes, 100 entry-state bytes,
current stack/PC and I/O offsets. Primary may correctly have eligibility=1.
Propose one separately reviewed, default-off test-build #UD at that checkpoint.

Expected: parent vector then genuine secondary vector 6, no vector 8; profile R-N
on the fatal stack, secondary CPU ESP=pre-secondary ESP-12, counter exactly 1,
auxiliary magic/kind 2/vector 6 and bounded fixed reentry message. Primary and
entry-state bytes stay identical. No primary emitter/formatter resumes and no
new ATA/CMOS writer is called. Do not clear the parent's eligibility field.
PASS requires exact preservation and terminal route; overwritten parent, recursive
C diagnostics, missing real secondary event or any writer fails. Missing exact
pre-injection snapshot/checkpoint is UNVERIFIED.

### Tier 2 — R2: before versus after diagnostic ESP replacement

Two distinct proposed variants, not a half-executed MOV:

1. After parent latch/vector/kind stores, before the diagnostic ESP replacement.
2. After ESP replacement/alignment, before the first C call.

Entry: parent frame already delivered, latch=1, healthy current SS and enough
space for one secondary #UD frame. Capture primary/entry bytes before injecting
one secondary event. Variant 1 must halt on the remaining boot stack; variant 2
on the fatal stack, both TR28/profile R-N. The secondary reduces its current ESP
by 12; the assembly route adds no software stack frame.

Primary magic may remain zero; preserve the actual bytes and report fields as
incomplete. The raw parent hardware frame can be retained as separately labelled
host evidence. Counter/auxiliary event must be genuine and unique. PASS does not
mean complete parent capture. Wrong expected stack, new C capture, a reset/DF or
fabricated publication fails; inadequate secondary frame capacity is UNVERIFIED
and must stop before injection.

### Tier 2 — R3: copied body before publication magic

Entry: exact `em_record_capture` destination is the primary record; its body copy
has completed but magic is still zero. Use instruction-level data-flow/DWARF
proof, not the source-line number or a generic memcpy breakpoint. Snapshot all
128 bytes, then propose a single #UD while still on a roomy fatal stack.

Expected: R-N; primary bytes unchanged with magic zero, auxiliary published and
counter=1, no C retry and no writes. Primary contents remain semantically
UNVERIFIED despite a successful byte-preservation result. Wrong destination,
published/replaced magic or repeated capture fails. A true fault halfway through
optimized memcpy requires different intrusive instrumentation and is deferred.

### Tier 2/3 — A2: bounded crossing into an inactive reservation (CONDITIONAL)

Requires explicit permission for a small, known RAM reservation delta. Entry:
prove lower bootstrap reservation [B-4096,B) has no objects and is mapped; source
and adjacent page tables are intact. A single #UD at pre-ESP **B+8** writes a
12-byte hardware frame ending at CPU ESP **B-4**. Exactly four bytes can lie in
the reservation; no page-table byte can be touched. No software push follows on
that old stack before switching. Mask interrupts and allow no other injector.
Apply A1's original-live-stack/nonoverlap proof as well, and independently verify
the complete hardware-frame write mask before authorizing the reservation delta.

Expected: ordinary #UD, not #PF/#DF; profile O with invalid/unclassified guest
frame and eligible=0. Guest refuses to decode the straddling frame; host raw-frame
evidence is labelled separately. The four reservation bytes may contain the
expected saved EIP, and no other unapproved byte may change. This is NOT a guard
protection fault; a PF-specific EM_GUARD_FAULT flag is not required.
PASS requires the exact bounded delta and unsafe halt. Any metadata overwrite,
extra frame, DF, eligibility or writer fails. Without a proven safe reservation
and approved byte mask, DEFER/UNVERIFIED; do not shift farther down to improvise.

### Tier 2/3 — P2: first fault while a real ATA READ is active

Entry: use a valid fixed read buffer and a fixture-owned sentinel sector outside
all recovery/package/slot/TinyFS ranges. At a proved point after READ command
submission and before transfer/completion, verify the real io_depth>0, latch=0
and a healthy bootstrap source stack. One #UD abandons the driver; do not manually
toggle depth. Record the submitted operation's direction/LBA/command identity.

Expected: profile O with EM_IO_ACTIVE/noneligibility, no new panic writer, no
CMOS fallback. The already-issued read may complete; no data/media write is
permitted and the whole image stays equal. Depth may remain nonzero because the
abandoned caller never executes its decrement. PASS requires no new command/data
write/flush after fault onset, with any completion correlated to the prior read.
Wrong I/O phase, unowned operation or changed disk fails/blocks interpretation;
an unproved active interval is UNVERIFIED, not an io-depth test PASS.

### Tier 3 — U1: absent or discarded serial output

Entry: independent post-clean-boot CPU/RAM readiness from section 3.2; same known
DF reference trigger, no active storage I/O. Two separately approved profiles:

- **Absent:** no emulated UART at COM1, independently check the QEMU device tree
  and qualified C presence state=0. Minimal C emitter must make no status/data
  I/O when its sink immediately reports absence.
- **Discard:** a present UART with a discard backend (e.g. a verified null backend),
  qualified presence=1. Emitted bytes may be unobservable at the console while
  real UART writes/counters are recorded; this is not a THRE-timeout experiment.

Expected: profile D with the unchanged Stage 2A hardware entry/task/RAM requirements,
stable DF halt, no writes; absence/drop of serial is the named device condition,
not proof of success. Require an independent new UART-negative profile to validate
RAM and I/O counters without deleting/reclassifying the Stage 2A baseline serial
assertion. PASS requires device condition and CPU/RAM halt; serial-only readiness,
wrong device model or unavailable observer is UNVERIFIED. Failure to halt by the
deadline, unexpected normal writer or wrong task fails.

### Tier 3 — U2/U3: live busy and poll-boundary/partial output (BACKEND-GATED)

Do not claim stock QEMU command-line options can deterministically hold THRE low.
First require separate review of a bounded host device model/controller that
changes only relevant UART status behavior, returns every I/O promptly and exposes
independent counters. No kernel-loop replacement or generic I/O shim affecting
ATA/RTC is acceptable. Without that model proof these profiles are DEFERRED.

Entry: healthy boot/presence=1; reference DF record published; pause before its
first emergency sink poll and acknowledge the model transition. Count within the
minimal emergency-emitter window, excluding fixture/boot normal serial calls.

- **U2 busy:** THRE remains zero. Require <=4,096 reads, zero accepted data writes,
  no subsequent sink calls after failure and profile D terminal halt.
- **U3 boundary/partial:** readiness only on the final allowed poll for a specified
  physical byte, then permanent busy for the next; optionally choose the CR/LF
  pair boundary. Exact byte/poll counters and accepted prefix must match source
  budgets; no retries after sink failure. Overall source bounds in section 1.5
  and the independent 30-second deadline remain mandatory.

PASS requires actual model acknowledgement, bounded I/O and CPU/RAM halt with
zero unsafe persistence. Excess polls/writes, changed peripheral semantics,
continued execution or timeout fails. Counter loss or an I/O operation blocked
inside the host model is UNVERIFIED/model failure, never guest-budget PASS.

### Tier 3 — R4: secondary fault after DF publication (CONDITIONAL)

Entry: reproduce accepted DF delivery; after DF latch=1 and record magic published,
stop at a roomy DF output checkpoint. Preserve the full DF/primary/fixture records,
normal saved TSS and DF backlink/busy state. Require the secondary #UD frame to
fit entirely within current DF payload. Propose one secondary #UD only.

Expected CPU sequence [14,8,6], no new #DF; profile R-D, not profile D. Secondary
raw flags save NT=1; live interrupt/reentry terminal NT=0, TR stays 30, ESP is
pre-secondary ESP-12. DF record remains byte-identical; primary remains as it was
(normally zero), auxiliary kind 2/vector 6 and counter=1. No return/IRET, task save
again, busy reset or storage request. PASS requires all distinct-profile evidence.
Any repeated DF/delivery fault, wrong NT provenance, replaced record or writer
fails. Pre-latch or unpublished-DF checkpoint ambiguity is UNVERIFIED and deferred.

### Tier 3 — R5: explicit CR2 preservation challenge (CONDITIONAL)

Only after R1 passes under separate approval. Entry: a parent ordinary PF has
successfully delivered and published CR2=its known address; current fatal stack
has room for one 16-byte secondary frame. One separately approved secondary read
from a distinct independently unmapped address occurs at the selected output
checkpoint. No table or guard mapping changes.

Expected [14,14], TR28/R-N and no vector 8: the first delivery is complete before
the second PF, unlike Stage 2A's hardware-delivery overlap. Primary CR2 remains
the first address byte-for-byte; current/auxiliary observed CR2 is the second,
without inventing a complete secondary record frame. Require counter=1 and no
writer. A DF, wrong primary CR2, invalid secondary frame extent or continuation
fails. Without independent publication/delivery-complete proof, UNVERIFIED.

### Tier 4 — U4 and P3 combined/in-flight profiles (separate later decisions)

**U4, reentry output under busy/absent UART:** only after the corresponding R and
U profiles pass. Use their one-secondary event, independent oracle and bounded
model. Require the reentry assembly's <=64 bytes/4,096 polls per byte, correct
R-N or R-D task/NT/frame state and unchanged parent evidence. For absent floating
bus, assembly may write bounded bytes despite the C presence flag being false;
do not require zero writes as the C profile does. Missing serial is expected only
under the approved model. Poll overflow/task return/writer fails; unvalidated
combined model is UNVERIFIED/DEFERRED.

**P3, active ATA WRITE/FLUSH:** only after P2 and a separately accepted operation
ledger. One raw sentinel-sector operation uses an explicitly fixture-owned LBA
(for example 2047 in a verified 2048-sector image, outside current fixed formats)
and known payload; never write recovery/state/package/slot/TinyFS sectors. Propose
one #UD at a precisely specified after-data-submission or after-flush-submission
checkpoint while real depth>0. No new recovery/panic command may be issued.

An already-submitted write/flush may finish after the fault. Acceptable data delta
must be specified before execution from the exact QEMU operation model: only the
known old sector or known complete payload, correlated to the already-owned
operation, and all other bytes unchanged. Do not allow arbitrary bytes simply
because they lie in the sentinel sector. PASS also requires profile O halt,
IO-active rejection and no new writer/CMOS request. Unexpected partial/torn data,
new command or ledger ambiguity fails or leaves persistence UNVERIFIED; never
relax image equality retrospectively. Metadata-transaction and interrupted panic
CMOS sequences are not included in this raw sentinel profile.

## 5. Complete emergency persistence inventory and operation ledger

### 5.1 Reachable write edges in the accepted code

| Edge | Source and effect |
| --- | --- |
| Ordinary entry to exception formatting | `emergency.c:108-114`: primary capture/emission, authorization, exact fixed-frame dispatch; `panic.c:112-129` rechecks authorization |
| Ordinary call panic formatting | `emergency.c:116-120` bounds/copies strings; `panic.c:73-84` authorizes formatter |
| Shared panic persistence edge | `panic.c:56-61` checks `emergency_persistence_allowed_now()` then calls mark-panic and journal append |
| CMOS request path | `recovery_memory_mark_panic:258-262` -> `recovery_memory_save:75-114`: nine data writes at CMOS 0x60..0x68 via ports 0x70/0x71 |
| State disk request path | save calls `recovery_store_save` when boot ordering trusted; `recovery_store.c:483-538` serializes and writes mirror LBA50 then primary LBA1 |
| Panic journal path | `recovery_journal_append:577-668` reads/modifies its journal sector, writes one 16-byte entry within LBA2..15, then saves state again |
| Physical ATA write/flush edge | `ata_write_sector:256-263` wraps attempts/retry; write implementation sends CMD_WRITE 0x30, data, then CMD_FLUSH 0xe7 and waits |

`error_log_class` is bounded RAM ring/string work plus serial output, not a hidden
disk journal. Page-fault recording is RAM metadata. CPL3 logging is not an eligible
ordinary path here. TinyFS, snapshots, package/slot writers exist as normal APIs
but are not called by the minimal DF/reentry/unsafe-entry graphs. Instrument all
ATA commands and CMOS writes anyway; any unexpected edge is failure, not a reason
to enlarge the permitted write set.

The lower-level RecoveryMemory/Store/Journal APIs do not themselves enforce a
universal emergency-context barrier. RecoveryMemory can write CMOS even when
disk save is disallowed. The guarding panic edge must stop the call before BOTH.
Current ATA depth covers actual identify/read/write/retry/flush intervals, not
all higher-level transactions or every possible CMOS sequence.

### 5.2 Timing and allowed-write sets

Record separate boundaries: settled boot, explicit arm, operation submission,
actual first/secondary CPU fault, handler entry, terminal observation and host
close. The unsafe no-new-request window starts at the relevant CPU fault, not at
an earlier keyboard command that intentionally submits the test operation.

- **No in-flight write:** zero new ATA write/data/flush and zero guest-initiated
  CMOS writes; complete image equality and recovery CMOS block equality required.
- **Active read:** the read/completion may be owned pre-fault; it cannot justify
  any data/media write. No new command after fault; whole image remains equal.
- **Active write/flush:** distinguish already-owned completion from newly initiated
  work using command direction/LBA, data extent and correlated submission/completion
  events in one ordered emulator stream. Only the preapproved operation's exact
  delta is allowed. An event after the fault is not automatically a new request.
- **Healthy ordinary control:** Stage 1's intentional panic state/one PANICT append
  and supported checksum/generation changes are authorized only for its positive
  control. Do not demand zero writes there or copy that permission to unsafe cases.

Maintain positive sensor controls from boot and event/counter integrity. A full
image hash does not detect an idempotent write or prove CMOS safety. Clock/status
registers may update autonomously; distinguish those from guest I/O data writes.
Preserve the recovery CMOS block and monitor all guest-initiated CMOS data writes,
rather than claiming equality of time-of-day registers that naturally advance.

## 6. Explicit deferrals — not sufficiently bounded for the first tranche

| Candidate | Why deferred / prerequisite for a separate review |
| --- | --- |
| Genuine #SS stack-capacity fixture | Flat current SS and mapped guards do not generate #SS merely at a payload boundary. A valid test-only expand-down SS could bound writes without PTE edits, but needs a separately reviewed immutable descriptor/selector allocation and proof that fatal/DF payloads remain addressable. Current common entry inherits SS. Loading an invalid selector often gives #GP, not the intended #SS; a nonpresent stack descriptor gives different #SS(selector) semantics. Do not corrupt production GDT to improvise it. |
| Fault before normal latch acquisition or before DF BTS | Another event may win primary ownership; original information may never be captured. Requires explicit evidence-loss/ownership-race design, not a promised ordinary reentry PASS. |
| Fault during DF hardware task delivery/error push, or forced second #DF while task busy | Can triple fault before software can act; not a bounded handler-reentry test. |
| Fault inside optimized memcpy or auxiliary publication, tertiary fault storm | Requires separately proved instrumentation/effects; auxiliary first-secondary publication is not a general atomic third-fault protocol. |
| Higher-level RecoveryStore transaction gaps, interrupted CMOS sequence, simultaneous persistence and reentry | ATA depth can be zero between sector operations; existing checks do not prove arbitrary half-mutated metadata safe. Requires transaction-state/barrier and allowed-prefix design, not forged flags or a blanket zero-write claim. |
| Busy UART without a deterministic returning backend; truly blocking I/O/host device | Guest finite polling cannot bound a host operation that does not return. Build/hash/model and independent-counter review is required first. |
| Corrupt SS/DS/CR3/GDT/IDT/TSS, machine-check/NMI storms, physical hardware, recursion | May lose entry before fixed stores, damage metadata or repeatedly consume frames. Outside this proposal; no general overflow/reentry claim follows from selected cases. |

For #SS-specific later design, an expand-down lower limit at B-1 with base zero
and 32-bit upper bound can conceptually make a push below B fault before RAM is
written, while higher diagnostic stacks remain legal. #SS during #SS delivery is
a contributory/contributory DF combination; an ordinary completed #SS delivery
is instead an ordinary unsafe exception. This is architectural analysis only,
not an implemented descriptor or an executable fixture specification.

## 7. Exception outcome matrix and stopping rules

| Condition | Expected architectural class / decision |
| --- | --- |
| Low payload headroom but complete frame mapped/legal in SS | Ordinary #UD/#PF; source policy rejects persistence; not automatically DF |
| Frame extends only into mapped inactive reservation | Ordinary delivery still possible; guest frame bounds reject it; no guard #PF |
| First #PF, frame push faults with #PF | Genuine #DF as accepted Stage 2A; task gate can supply new stack |
| Benign #UD delivery encounters #PF/#SS | May attempt that second exception first; not every pair is immediately DF; do not assume Stage 2A trace |
| Two contributory faults during delivery, e.g. #SS during #SS | DF combination, if a separately proved SS fixture establishes it |
| New ordinary fault after a handler/task delivery completed, stack still valid | Separate delivery; busy latch may route reentry; not automatically DF/triple fault |
| DF delivery fails, or later failure requires a busy DF gate again | Possible triple fault; unacceptable for ordinary/reentry PASS |

Immediate stop on wrong vector sequence/task/stack, ordinary fallback wrongly
counted as success, changed published evidence, recursive capture/logging, new
writer outside ledger, counter/trace loss, unplanned device behavior, reset,
triple fault, unexpected task return/continuation, failure to remove a debugger
breakpoint, deadline or source/evidence mismatch. Do not retry another checkpoint,
clear busy state, expand a write mask or weaken a negative control after failure.
Quarantine the full failed run and request review. UNVERIFIED blocks progression.

## 8. Evidence protection and authorization decisions

Accepted source, original recovery image, all audit archives and all prior report/
run trees are read-only baselines. Maintain before/after content inventories and
all-ref snapshots. Preserve all original ELFs, including the original failed
Stage 2A observation and exit code; no log is overwritten or relabelled.

Future cases may attach only newly created images or explicit copies/copy-on-write
children inside a fresh case directory. Never attach the original recovery image
or an accepted historical test image. Declare every writable path and sentinel
sector, preserve pre/post full images, and keep any next-boot analysis on another
copy with fault activation absent. Boot state/journal/snapshot writes must settle
before unsafe-window comparisons. Format/authority/TinyFS regions stay protected.

Separate approval is required for:

1. Exact first implementation tranche and each case/parameter/checkpoint; initial
   recommendation is H0, A1, P1 and R1 before the more complex cases.
2. Any test-build-only injection hook, arm state, source-stack pivot, RAM-byte
   allowance or new entry/terminal observer profile; never direct writer calls as
   a proxy for an unsafe exception, or live code/metadata patching.
3. Serial-independent boot/activation observation and UART-negative acceptance
   profile; Stage 2A's original assertions and historical statuses stay immutable.
4. Any custom UART backend/model, poll aggregation and its independent controls.
5. R-D's distinct NT/provenance requirements and any combined failure profile.
6. Active-read phase and especially active-write/flush sector/payload/completion
   ledger, plus any positive ordinary-panic persistence control.
7. Every deferred SS/early-capture/publication/transaction experiment, after its
   missing architecture/model proof is supplied. No broad approval is inferred.
8. Fresh repetition, regression selection and independent acceptance after a
   successful case; no authorization to start Phase C/D or production protections.

## 9. Readiness conclusion

The case catalogue, route-specific oracles, risk boundaries and explicit
deferrals are ready for design review. **No Stage 2B implementation or execution
is authorized or performed by this document.** Current status of every new case
remains UNVERIFIED—DESIGN ONLY; historical Stage 1/2A acceptance is preserved.

STAGE2B_DESIGN_READY_FOR_REVIEW
