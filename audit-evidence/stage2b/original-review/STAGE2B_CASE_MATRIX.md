# Stage 2B adversarial case matrix

All statuses concern **design readiness for possible owner consideration**, not
permission to implement or execute. Every new runtime outcome is UNVERIFIED.
No case or control was run during this review. References use D/K/P/A as defined
in `STAGE2B_INDEPENDENT_REVIEW.md`; findings F01-F06 are in that report.

Common non-negotiable oracle: genuine planned CPU events/site/frame evidence,
correct task/selectors/CR3/stack, exact publication/provenance, exact-ELF terminal
loop with HLT=1 and IF clear, stable PC/ESP >=250 ms, no return/continuation,
independent reset/triple-fault/deadline detection, frozen RAM/storage allowances
and source/evidence preservation. A quiet device or debugger stop alone is never
positive evidence. Stage 2A's original observer/assertions remain unchanged.

## H0 — READY FOR AUTHORIZATION REVIEW (narrow host-data core only)

1. **Invariant:** route/publication/boundary observers reject false mechanisms
   without guest execution (D:306-315).
2. **Preconditions:** exact P/A ELF identities, separate O/R-N/R-D contracts;
   explicit truth tuples and no device/guest hooks. UART-model controls are outside
   this ready core until their model is specified.
3. **CPU/RAM behavior:** input data only. Distinguish frame-valid from eligible,
   magic-zero from published, correct task from a merely similar halt PC.
4. **Positive evidence:** accepted existing build witness and correct synthetic
   tuples, including eligible parent/auth=0 for R1 and correct partial-publication
   byte handling; these do not become hardware results.
5. **Negatives/false positives:** wrong EIP/TR/SS/CR3, dispatcher-return HLT,
   unauthorized-formatter shortcut, debugger-only stop/no secondary vector, bad
   raw frame/counter, parent byte change, missing sensor/reset and short stability.
6. **Allowed effects:** new host control inputs/results only; no guest RAM/image,
   original report or source mutations.
7. **Failure/unobservability:** accepting an incomplete/misclassified mechanism,
   replaying a historical result as fresh, or unspecified expected tuple blocks it.
8. **Missing detail:** freeze the finite catalogue and expected outcomes; F01-F05
   provide the necessary core distinctions. Do not silently include backend work.
9. **Tranche:** suitable as a separate first host-data-only implementation proposal;
   this review neither requests nor grants that authority.

## A1 — REVISE DESIGN

1. **Invariant:** a complete legal hardware frame with inadequate source policy
   headroom reaches the diagnostic stack and is denied persistence (D:317-339).
2. **Preconditions:** B/T from exact ELF; flat 32-bit SS; CPL0/known flags/gates;
   accessible source/F/DF memory; old-live-frame nonoverlap; no post-pivot spill;
   all admission predicates except headroom true, including trusted idle storage.
3. **CPU/RAM behavior:** S=B+F yields CPU ESP B; S=B+F+1020 yields B+1020.
   F12/F16 frames fit wholly inside payload. No guard/table corruption is needed.
4. **Positive evidence:** real vector/site/raw words, reconstructed origin S,
   frame_valid=origin_valid=1, BAD_STACK=4 with no unrelated rejection reason,
   eligible=0; TR28/fatal stack/exact unsafe terminal, no DF/reentry/writer.
5. **Negatives/false positives:** wrong frame size, source escape, invalid origin,
   unavailable store masking a headroom bug, eligible result, ordinary dispatcher
   fallback, correct-PC-but-running CPU or unexpected DF.
6. **Allowed effects:** exact source frame interval; normal bounded capture/F-stack/
   latch/record effects and CPU A/D/accessed effects separately declared. Disk and
   recovery CMOS unchanged. No reservation/table delta.
7. **Failure/unobservability:** extra pre-switch push/call, bad mapping/SS or hidden
   invalid context can escalate/fail admission; treat as failure/block, not rescue PASS.
8. **Missing detail:** F01 exact validity predicate and F02 phase-specific RAM
   masks/baselines. The written "valid/eligible" failure rule is not deterministic.
9. **Tranche:** after revision, start with one #UD/S=B+F+1020 case; boundary/PF
   parameters are separate later variants. Not implementation-ready as one broad A1.

## P1 — REVISE DESIGN

1. **Invariant:** unavailable/untrusted storage must not cause a panic CMOS fallback
   or writer invocation (D:341-360).
2. **Preconditions:** valid otherwise-ordinary frame, runtime/DF ready; choose one
   exact no-controller/no-disk or locked-image configuration and expected initialized
   trust fields. No forged flags; settle boot operations.
3. **CPU/RAM behavior:** ordinary #UD/TR28/fatal stack; valid record with specific
   storage rejection and unsafe terminal. ATA absence and metadata lock are different
   conditions, not interchangeable configurations.
4. **Positive evidence:** actual availability/lock state, eligible=0 and expected
   path; forbidden writer-edge proof/observations plus valid CMOS measurement chain,
   not merely zero events from unavailable hardware.
5. **Negatives/false positives:** writer API entered but lower guard returned;
   disabled trace mistaken for zero writes; wrong lock/other invalid predicate;
   serial warning mistaken for initialized trust; boot writes in the fault window.
6. **Allowed effects:** normal unsafe capture/F-stack effects. No new persistent
   request; locked image exact equality; no-disk image equality is N/A, not a media
   denial result. Recovery CMOS equality needs F04's defined measurement.
7. **Failure/unobservability:** auto-repair changes the seed, no usable lock snapshot,
   absent-device inability to calibrate a writer sensor or missing CMOS state proof.
8. **Missing detail:** F04-F05; name the accepted seed recipe/fields, causal no-call
   oracle and per-profile positive calibration before selecting the case.
9. **Tranche:** later than corrected A1; do not authorize the combined unavailable/
   locked family on the strength of an absence-only pass.

## R1 — REVISE DESIGN

1. **Invariant:** one genuine secondary fault after publication preserves the first
   record and diverts before permitted ordinary persistence (D:362-377).
2. **Preconditions:** choose one parent vector/site; primary address/magic/fields
   proven, eligible=1, store ready/idle, ordinary_authorized=0, latch1/counter0,
   current F stack with a full secondary 12-byte frame extent. Pin one emitter point.
3. **CPU/RAM behavior:** completed parent delivery then #UD i=0 at the new point,
   TR28; current ESP drops 12; auxiliary-only assembly runs and never resets ESP.
4. **Positive evidence:** exact two-event/site sequence, pre-secondary 128/100-byte
   snapshots, unchanged parent, counter1, auxiliary published kind2/vector6/control
   fields, raw secondary frame, exact R-N halt and no writer entry/request.
5. **Negatives/false positives:** direct guard jump to same reentry HLT, debugger
   trap with no guest fault, wrong record passed to generic emitter, already denied
   parent, replaced parent, counter0/2, missing frame or continuing primary formatter.
6. **Allowed effects:** secondary hardware frame, auxiliary record/counter, owned
   fixture arm and CPU effects; parent bytes immutable. Disk/CMOS no new writes.
   An interrupted parent serial prefix may be incomplete by the fixed checkpoint.
7. **Failure/unobservability:** source/register/stack changes by the hook, a second
   frame that does not fit, wrong phase or another exception/DF during output.
8. **Missing detail:** F03; fix the named parent, one checkpoint and parent admission
   tuple. H0 controls must include fake reentry and eligible-parent/auth0 positives.
9. **Tranche:** not contingent on first running R2; its own actual published/F-stack
   evidence proves the entry state. Reconsider after H0 and ordinary profiles, not
   as an unspecified fourth item in the current batch.

## R2 — REVISE DESIGN

1. **Invariant:** latch ownership prevents C recapture on either side of ESP
   replacement, while preserving whatever entry bytes already exist (D:379-397).
2. **Preconditions:** parent delivered, latch1, correct SS/DS and writable globals,
   known current stack; select one point just before MOV ESP and one just after
   alignment/pre-call, each a separate case, not an interval.
3. **CPU/RAM behavior:** before switch, secondary frame on boot stack; after switch,
   on F stack. Final ESP is the secondary hardware ESP, not either stack top. TR28.
4. **Positive evidence:** instruction-accurate pre-secondary CPU/RAM, real vector6,
   12-byte extent, unique counter/auxiliary publication, unchanged partial parent/
   entry bytes and exact stackless terminal.
5. **Negatives/false positives:** selecting before ownership, accepting F top for
   a boot-stack case, treating zero magic as valid capture, or accepting a generic HLT.
6. **Allowed effects:** declared secondary frame/auxiliary/counter/fixture effects;
   existing partial bytes immutable; no persistence or first-record reset.
7. **Failure/unobservability:** entry assembly can still fault on frame delivery,
   DS-based stores, literals or I/O. Pre-switch hook spills can invalidate the case.
8. **Missing detail:** F06 exact two points and per-point partial-state/write mask;
   bare "after stores/before replacement" spans materially different data states.
9. **Tranche:** later than late-published R1; earlier in execution does not mean safer.

## R3 — REVISE DESIGN

1. **Invariant:** copied-but-unpublished body is preserved without inventing a
   complete first record (D:399-410).
2. **Preconditions:** destination is primary, not DF/another memcpy target; body
   copy complete, magic0, latch1, roomy F stack; exact publication instruction.
3. **CPU/RAM behavior:** P's memcpy returns before magic store at 0x1037be; a single
   #UD before that store takes R-N. No primary retry/publication resumes.
4. **Positive evidence:** actual destination/data-flow identity, full pre-image,
   byte-identical magic-zero parent, raw secondary frame, auxiliary kind2/counter1
   and stable reentry halt; record semantic fields remain UNVERIFIED.
5. **Negatives/false positives:** generic memcpy hit, wrong destination, body not
   complete, publication already occurred, or interpreting body flags as valid
   despite zero magic.
6. **Allowed effects:** aux/counter/secondary frame/fixture and bounded CPU effects;
   no persistent writes. "No writes" cannot forbid the mandatory RAM effects.
7. **Failure/unobservability:** compiler inlining/merging removes the assumed
   checkpoint, hook clobbers destination, or a mid-copy failure is substituted.
8. **Missing detail:** exact future-build pre-store witness, normal RAM mask/common
   CMOS oracle (F02/F04) and positive versus invalid publication controls.
9. **Tranche:** mechanism is bounded in principle, but later than R1 and not a
   justification to instrument the memcpy implementation itself.

## A2 — DEFER

1. **Invariant:** distinguish a mapped reservation crossing from actual guard
   protection; guest declines a straddling source frame (D:412-430).
2. **Preconditions:** approve the reservation byte mask, prove object-free mapped
   [B-4096,B), old-frame nonoverlap, valid SS, no extra pushes/interrupts.
3. **CPU/RAM behavior:** S=B+8, #UD F12 gives ESP=B-4; four EIP bytes can land in
   reservation. It is still ordinary delivery, not inherently PF/DF.
4. **Positive evidence:** exact raw 12-byte frame and four-byte delta, no other
   source/table changes, guest frame invalid/unclassified, eligible0 and O halt.
5. **Negatives/false positives:** expecting EM_GUARD_FAULT for #UD, shifting farther
   down after failure, accepting a different invalid frame or hiding table corruption.
6. **Allowed effects:** that four-byte reservation delta plus full declared frame/
   normal capture ownership masks; disk/CMOS unchanged. Not broad guard-page damage.
7. **Failure/unobservability:** bad bounds, accidental spill/extra event, wrong
   active-frame mask or a reservation occupied in the new build.
8. **Missing detail:** F02's complete phase/RAM contract and successful ordinary
   unsafe-path controls before conditional destructive RAM allowance.
9. **Tranche:** not needed to establish first low-headroom result; defer until A1
   semantics and bounded mutation observation are established.

## P2 — REVISE DESIGN

1. **Invariant:** actual driver I/O interval prevents new panic writes, while an
   already-owned read completion is not misclassified (D:432-446).
2. **Preconditions:** exact owned sector/buffer, READ submitted, real depth>0,
   latch0 and healthy boot/F stacks; freeze one PIO checkpoint.
3. **CPU/RAM behavior:** #UD first fault -> O with IO_ACTIVE rejection. Depth may
   remain1 because the abandoned caller never decrements it. Backend read completion
   and software PIO/API completion are not the same event.
4. **Positive evidence:** command/LBA/transfer-count/controller phase plus linked
   PC, depth and raw fault frame; no new command/write/flush/CMOS; equal image.
5. **Negatives/false positives:** forged depth, a merely delayed wrapper return,
   unrelated operation, ordinary I/O error instead of guest exception, zero writes
   without evidence that the intended READ interval was reached.
6. **Allowed effects:** approved read buffer/driver state before fault, unsafe
   capture after; owned read may complete but cannot justify a media change.
7. **Failure/unobservability:** ATA had already completed the intended phase, new
   retry/command cannot be attributed, invalid buffer or bad I/O timing.
8. **Missing detail:** F06 post-command/pre-first-INW or similarly precise phase;
   settle which kind of "active" is claimed and freeze the operation boundary.
9. **Tranche:** after ordinary storage/unsafe profiles; revision needed, though a
   single PIO read is substantially safer than a pending write experiment.

## U1 — DEFER

1. **Invariant:** no-console availability/delivery does not prevent emergency
   terminal progress (D:448-466).
2. **Preconditions:** separately reviewed serial-independent readiness at the exact
   shell entry after boot operations; actual absent COM1 or present discard backend,
   qualified presence state and known DF reference entry.
3. **CPU/RAM behavior:** C absence returns failure without port polls; present/null
   may transmit while host observes no text. Neither proves permanent THRE busy.
4. **Positive evidence:** actual device tree/register condition, CPU/RAM boot
   predicates, consumed arm, genuine DF/task entry and stable DF terminal; I/O
   counters and published ineligible record replace silence as evidence.
5. **Negatives/false positives:** serial boot timeout accepted as boot, null backend
   called absent/busy, wrong duplicated `present` symbol, stopped/debugged CPU
   mistaken for progress, original Stage 2A serial predicate silently removed.
6. **Allowed effects:** normal RAM DF evidence only after boot; no persistent writes.
   Keep original healthy baseline immutable with a distinct UART-negative test ID.
7. **Failure/unobservability:** no independent arm/boot rendezvous, blocked backend,
   ambiguous status/counters or missing raw CPU state.
8. **Missing detail:** concrete new readiness/acceptance profile and F04 CMOS method;
   choose exact device/backend separately, not one broad absent-or-discard pass.
9. **Tranche:** appropriate later only after observer and observation-channel review.

## U2 — DEFER

1. **Invariant:** real UART THRE-busy output exhausts a bounded poll budget then halts.
2. **Preconditions:** returning deterministic backend, hashes/counters reviewed;
   presence1, published DF record, stopped before first emergency physical poll.
3. **CPU/RAM behavior:** first minimal byte fails after <=4096 status reads; no
   later sink requests; CPU proceeds to the normal DF terminal (D:468-491).
4. **Positive evidence:** model acknowledgement, actual CPU IN/status behavior,
   independent poll count and stable task/RAM terminal.
5. **Negatives/false positives:** absent flag short-circuits without testing busy;
   backend block mistaken for a guest loop; counting normal preamble calls;
   accepting no serial bytes as proof.
6. **Allowed effects:** UART model state/counters and DF RAM effects only; no storage.
7. **Failure/unobservability:** unsupported QEMU status control, nonreturning I/O,
   dropped counter/trace events or whole-run timeout.
8. **Missing detail:** the design deliberately leaves the backend/model unselected.
   Ordinary null/none CLI configurations are not this model.
9. **Tranche:** correctly backend-gated; not a first implementation authorization.

## U3 — DEFER

1. **Invariant:** last-allowed-poll success and then partial-output failure obey
   per-physical-byte budgets and stop on failure (D:482-491).
2. **Preconditions:** U2 model validated; one exact physical byte/prefix/CR-LF
   boundary, per-byte counter reset rule and mode transition pinned.
3. **CPU/RAM behavior:** readiness on final permitted poll succeeds; selected next
   byte remains busy; record stays intact and DF terminal is reached.
4. **Positive evidence:** exact accepted prefix and read/write counts, no subsequent
   sink calls, independent CPU/RAM terminal and zero new persistence.
5. **Negatives/false positives:** logical/physical character confusion, resetting
   counts on observation instead of actual byte, allowing an arbitrary output prefix.
6. **Allowed effects:** predefined UART bytes/model counters plus DF RAM effects;
   no ATA/CMOS writes or weakened record requirements.
7. **Failure/unobservability:** no trustworthy byte identity, model timing drift,
   logging cap/drop or counters derived solely from the guest being tested.
8. **Missing detail:** precise U2 backend and event/counter protocol; freeze every
   boundary variant rather than one optional newline family.
9. **Tranche:** only after U2 and a separate boundary-case review.

## R4 — DEFER

1. **Invariant:** one completed secondary #UD in a delivered DF task preserves the
   published DF evidence and halts without task return (D:493-506).
2. **Preconditions:** genuine accepted DF delivery completed, latch1, DF publication
   verified, roomy current DF stack, known secondary checkpoint.
3. **CPU/RAM behavior:** [14,8,6], TR30; raw secondary frame saves NT1 but live
   gate/reentry flags have NT0. The busy DF TSS/backlink remains; no new task save.
4. **Positive evidence:** independent DF-entry proof then exact #UD/frame/counter,
   immutable parent DF/primary/snapshot/TSS state and R-D-specific terminal.
5. **Negatives/false positives:** reusing the uninterrupted NT1 oracle, reading NT
   from the DF record's old normal-task flags instead of the secondary frame,
   triggering before latch/publication, accepting a second DF.
6. **Allowed effects:** secondary frame/auxiliary/counter/fixture and normal CPU
   gate effects; no parent replacement, busy clearing or new persistence.
7. **Failure/unobservability:** secondary frame cannot fit -> delivery cascade;
   another DF needs the already busy task and can triple fault.
8. **Missing detail:** validated R-N core and independently reviewed R-D state
   tuples/negative controls. Architecture expectation is not observed Stage 2B data.
9. **Tranche:** later conditional experiment; do not put before the normal reentry cases.

## R5 — DEFER

1. **Invariant:** published first PF CR2 survives a genuine later PF at another
   address; current/auxiliary CR2 identifies the later fault (D:508-521).
2. **Preconditions:** first delivery completed and record published, valid F stack
   with 16-byte secondary extent, exact distinct unmapped read/point; R1 dependency.
3. **CPU/RAM behavior:** [14,14] as sequential deliveries, TR28; not automatic DF.
   The busy latch diverts the second ISR to assembly auxiliary capture.
4. **Positive evidence:** first publication snapshot, second CPU PF/header/error,
   primary first CR2 unchanged, later CR2/frame independently read, R-N halt.
5. **Negatives/false positives:** treating every two PFs as Stage 2A overlap, same
   address for both, reading current CR2 as original, or accepting an undecoded first
   record as complete publication.
6. **Allowed effects:** normal secondary/auxiliary/counter/fixture effects; primary
   unchanged and no persistence.
7. **Failure/unobservability:** first frame delivery not complete, source interval
   ambiguous or secondary push fails and escalates unexpectedly.
8. **Missing detail:** actual successful R1, exact second address/checkpoint and
   four-way CR2 provenance controls (CPU, primary, auxiliary, first trace).
9. **Tranche:** sensible later falsification case, not needed for the first tranche.

## U4 — DEFER

1. **Invariant:** assembly reentry output remains bounded under a separately proven
   UART fault, without replacing parent evidence (D:525-532).
2. **Preconditions:** corresponding R and U profiles passed; one fixed combination,
   independent observation and returning model; no third injection.
3. **CPU/RAM behavior:** R-N or R-D task/stack/NT state; <=64 bytes and <=4096 polls
   per byte. Unlike C, the assembly ignores presence and may output on floating FF.
4. **Positive evidence:** genuine secondary event, preserved parent/auxiliary data,
   model/status/counters and exact reentry terminal.
5. **Negatives/false positives:** expecting zero port writes merely because C says
   absent; borrowing C minimal-emitter budget rules; inferring halt from silence.
6. **Allowed effects:** selected UART/model plus R frame/auxiliary/counter effects;
   no persistent writes or abandoned-parent resumption.
7. **Failure/unobservability:** simultaneous unproved failure axes obscure which
   mechanism failed, backend blocks, or observation requires the failed console.
8. **Missing detail:** validated individual profiles and a fixed combination ledger.
9. **Tranche:** correctly late; do not use it to bootstrap either reentry or busy-UART proof.

## P3 — DEFER

1. **Invariant:** a previously submitted raw write/flush may finish, but the unsafe
   handler must initiate no additional persistence (D:534-549).
2. **Preconditions:** P2 passed, exact owned sentinel sector/payload/buffer and
   data-versus-flush checkpoint, real depth, no metadata/other writers; pre-fault
   operation identity and trace ordering fixed.
3. **CPU/RAM behavior:** #UD -> O/IO-active denial; already-submitted controller
   activity can complete after CPU halt without a new command.
4. **Positive evidence:** command/data/flush phase, correlated completion and exact
   approved target outcome; all unrelated image bytes, record and CMOS constraints.
5. **Negatives/false positives:** allowing every post-fault change, treating a new
   write as an old completion, or taking the baseline after the target already changed.
6. **Allowed effects:** only predeclared old/full-payload target result correlated
   to the prior operation; no recovery/authority/TinyFS sector or new request.
7. **Failure/unobservability:** partial/torn data, cache timing/flush model unclear,
   missing operation identity, submission boundary or post-fault completion proof.
8. **Missing detail:** actual controller/storage model and ledger, exact baseline
   moments, data delivery extent and atomicity assumptions. Higher-level transaction
   gaps remain outside this case and unproved.
9. **Tranche:** defer; never widen a mask after seeing an unexpected write.

## Summary

| Case | Design disposition |
| --- | --- |
| H0 core | READY FOR AUTHORIZATION REVIEW |
| A1, P1, R1, R2, R3, P2 | REVISE DESIGN |
| A2, U1, U2, U3, R4, R5, U4, P3 | DEFER |

No case is labelled permanently impossible: currently unresolved/device-gated
ones are deferred rather than assumed executable. Individual subclaims without
a measurement method remain UNVERIFIED, not a PASS. These classifications and
suggested repairs do not authorize implementation or execution.
