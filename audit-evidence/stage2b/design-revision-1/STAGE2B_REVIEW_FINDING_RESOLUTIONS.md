# Stage 2B Revision 1 — adversarial finding dispositions

This is the author's design response to the immutable six-file adversarial
review, not an independent acceptance. All seven authoritative inputs were
available and read; input hashes are recorded in provenance. No implementation,
control, calibration, test or VM was run. Every new case is UNVERIFIED.

The three permitted disposition labels concern **the design finding**, not a
runtime result. A resolved semantic contract can still have unprovided future
build/calibration evidence. Any such missing item is explicitly UNRESOLVED in
the prerequisite register and blocks the affected next authorization gate.

## Summary

| Finding | Disposition | Correction | Residual gate |
| --- | --- | --- | --- |
| F01 | **RESOLVED IN DESIGN** | One A1 #UD/S=B+1032 parameter; valid frame/origin1, isolated unsafe0x04, eligible0; publication/admission separated. | Future build/RAM/CMOS/W prerequisites; no runtime claim. |
| F02 | **PARTIALLY RESOLVED** | Six mutation classes, strict post-pivot baseline, exact source-frame bytes, record/entry field predicates, proposed bounded diagnostic budget and epoch-specific freezes. | **UNRESOLVED U-RAM:** exact future allocation/store/high-water/bit masks not derivable without an authorized build;4096 is a design cap, not proof. |
| F03 | **RESOLVED IN DESIGN** | R1 parent #UD at existing trigger, one first-primary-emitter/pre-prologue checkpoint, eligible1/auth0, caller/args/storage/stack tuple and genuine one-secondary UD2. | **UNRESOLVED U-BUILD/U-WRITER/U-CMOS:** new hook/bindings and live observation/calibration absent. |
| F04 | **PARTIALLY RESOLVED** | One operational CMOS-T1 complete-known-write/continuous-trace reconstruction method, explicit model audit/calibration steps, idempotent-write negatives. | **UNRESOLVED U-CMOS:** exact RTC source/binary coverage and live calibration not supplied. New-case equality remains UNVERIFIED. |
| F05 | **PARTIALLY RESOLVED** | Separate NC/ND/LK profiles, exact existing seed + ambiguity recipe/trust tuple, W forbidden-entry proof/observation and per-profile sensors. | **UNRESOLVED U-WRITER/U-DEVICE:** executable site/capacity coverage, NC/ND topology and applicable live calibration absent. |
| F06 | **RESOLVED IN DESIGN** | Single R2 pre-MOV and post-alignment/pre-CALL boundaries; single R3 pre-magic boundary; P2-CMD distinct from DRQ/transfer/wrapper/backend; P3 ledger deferred. | **UNRESOLVED U-BUILD/U-P2:** future binding/status observation; P3 gets no delta authority. |

## F01 — Correct A1 field predicates

**Review issue:** original design331–339 mixed structural validity and eligibility.
The slash in "valid/eligible classification" could reject the intended success.

**Source evidence:** `kernel/src/emergency_frame.c:35–55` independently bounds/
decodes a legal same-CPL frame and sets frame_valid/origin_valid. Persistence
headroom is a separate check in `emergency_record.c:12–17`; magic publication is
another operation at40–58. No source change is needed to distinguish them.

**Normative repair:** main revision section3 and oracle O1 pin A1-UD-1020:
hardware ESP=B+1020, origin=B+1032, raw12 bytes inside payload, published
version1/kind1/vector6/context1, frame_valid=origin_valid=eflags_known=1,
unsafe_flags=`0x04` exactly and persistence_eligible=0. Independently require
runtime/storage readiness1, ATA idle and all other admission conditions true.
Counter0/DF-zero, exact O unsafe halt and healthy numeric output are mandatory.

H0 HP01 and HN01–07 distinguish these properties; HN04 rejects BAD_STORAGE hiding
headroom, HN10 rejects dispatcher fallback. No DF rescue, frame-invalid outcome,
extra reason or eligible outcome can pass. Other positions/PF parameters are
deferred distinct cases. **Disposition: RESOLVED IN DESIGN.**

## F02 — Partition RAM writes and phase boundaries

**Review issue:** global "any write outside the frame" would reject mandatory
record/latch/diagnostic activity, while pre-keyboard baselines or broad new masks
could conceal unrelated corruption. R3's "no writes" also needed persistence scope.

**Source evidence:** `emergency_entry.S:18–61` writes fixed entry RAM and switches
ESP before calling C; `emergency.c:90–111` captures/emits on diagnostic stack;
reentry `interrupt_stubs.S:19–24`/`emergency_entry.S:87–131` writes auxiliary/counter
but makes no software pushes/C calls. `emergency_abi.h` fixes100/128-byte sizes.

**Normative repair:** mutation document M0–M6 separates hardware frames; fixed
capture/latch/counter; bounded diagnostic work; fixture arm/ack/buffer; CPU A/D/
descriptor/task effects; immutable unrelated RAM. E-fault is post-pivot/pre-UD2
with IF0, alongside E-live original-frame proof. R cases add stricter E-parent
snapshots. No R software-stack delta is allowed after its secondary baseline;
only the hardware frame, exact auxiliary/counter and declared CPU effects change.

P's fixed ranges and field values are specified. Source-frame delta is exact12
bytes, fixture arm/ack budget8 bytes, auxiliary sparse DWORD changes at most32
bytes +counter4. Proposed fatal software-stack budget is the top4096 bytes with
an independently required exact-store/high-water proof, never the whole16-KiB
payload. Page/table/GDT permission is per enumerated hardware bit, not a page mask.

**Remaining uncertainty:** no future instrumented ELF exists; actual fixture
addresses, complete stack-store/high-water proof, enumerated table/descriptor
words and normal-RAM capture coverage are **UNRESOLVED U-RAM/U-BUILD**. The written
cap is not proof of sufficiency, and a future unexpected delta cannot widen it.
Endpoint RAM equality is not a universal transient-write trace. HN08/09/48 reject
out-of-mask corruption. **Disposition: PARTIALLY RESOLVED.**

## F03 — Pin R1 publication, caller and authorization phase

**Review issue:** already-denied parents or a generic emitter/sink hit could mimic
diversion of an otherwise eligible path; "may be eligible" did not define one case.

**Source evidence:** `emergency.c:108–110`; P capture CALL `0x00102e58`, emitter
CALL `0x00102e6e`, return `0x00102e73`, authorization store `0x00102e8a`.
P emitter entry `0x001037e0`; first record `0x0013dbc0`. These are existing
reference bindings, not a claim that P contains the new secondary hook.

**Normative repair:** R1-UD-EMIT0 chooses the existing parent UD2
`interrupts_trigger_invalid_opcode_test` (P `0x00107c33`) on healthy bootstrap
stack, then C-EMIT0's first primary-emitter entry from that one ordinary caller,
before its original prologue. Require actual primary pointer/sink/context/return
tuple, published128/decoded100 snapshot, eligible1/unsafe0, storage1/depth0,
auth0/latch1/counter0, and a full secondary12-byte extent on the live fatal stack.

C-SEC is one genuine UD2 after the declared nonspilling arm/ack prefix immediately
before the original emitter instruction. Its exact linked numeric binding is
**UNRESOLVED U-BUILD**; changing to a later byte, different caller or ineligible
parent is a new case. No parent numeric prefix has yet been emitted. A real
[6,6] site/raw-frame sequence, counter1 and auxiliary publication, unchanged128/
100, exact R-N terminal and no writer/resume are required.

HP03 and HN24–34/45/47 cover phase/pointer/caller/debugger/shortcut/preservation
false positives. R1's own current stack and publication proof suffices;
**prior R2 execution is not required**, matching the independent tranche decision.
Live hook/W/CMOS evidence remains unavailable. **Disposition: RESOLVED IN DESIGN.**

## F04 — Operational CMOS equality chain

**Review issue:** QMP pmemsave does not capture RTC cells, disk equality is not
CMOS equality, and no direct recovery-cell snapshot/export had been justified.

**Source evidence:** `recovery_memory.c:28–40,75–114` selects indices via70 then
reads/writes data71; every save writes60..68, before disk conditional. Source
provides the9-byte format/checksum, not emulator observation coverage.

**Normative repair:** oracle CMOS-T1/C1 chooses one trace-reconstruction method:
pin/audit RTC model and source-to-binary correspondence; prove all cell-update
paths; identify a complete known nine-cell boot overwrite; reconcile values with
RAM/format/checksum; retain continuous calibrated index/value events and independent
lossless coverage through close; zero guest data writes plus proven no autonomous
recovery-cell changes implies reconstructed equality. All guest CMOS writes fail,
including idempotent/outside-block writes. Clock/status evolution is separate.
No invented QOM property/export and no guest read-side/NMI-mask alteration.

**Available evidence:** static `EXISTING_CMOS_TRACE_DATA.json` reads historical
accepted traces with the last block `4b 5a 01 00 01 00 00 00 59`, zero recorded
post-trigger recovery writes, and **no RTC model/binary coverage attestation**.
It is neither new calibration nor direct state export. HN20–22/41–43 reject
unqualified zero events/equality. **UNRESOLVED U-CMOS** blocks new-case equality
until independently reviewed calibration/model evidence exists.
**Disposition: PARTIALLY RESOLVED.**

## F05 — Eliminate vacuous unavailable-device success

**Review issue:** lower lock/presence guards may suppress hardware I/O after a
forbidden writer was called. Positive media-write calibration is inapplicable
to a no-controller/no-disk boot, while CMOS remains writable.

**Source evidence:** `ata.c:103–165` distinguishes ff/no-controller from ec/status0
no-device; `recovery_store.c:330–383,483–486,581–591` locks/returns; RecoveryMemory
save always writes CMOS. Existing test ambiguity recipe is
`tests/test_integrity_gate.py:158–179` and resealing `tests/kmz_disk.py:226–237`.
They were inspected as text, not imported/run.

**Normative repair:** main section5 chooses three IDs with exact initialized
tuples. P1-LK-UD is the first later storage candidate; it uses one preserved
1048576-byte, generation5/boot1/jcount3 seed, mirror boot200 resealed with both
checksums and equal generation. A copy has **not** been created. Source predicts
real ambiguity, valid0/locks1/scans1/1, RAM generation0, jcount3, trust0; fresh
invalid CMOS load plus journal boot lower bound1 gives boot_count2. Snapshot real
boot values before the fault. NC/ND topology/model specifics remain separate.

W1–W3 adds a forbidden-entry/call-edge registry with inline/indirect coverage and
<=16-site observation/capacity/calibration requirement; a hit fails even on early
return/no ATA event. Applicable sensors need positive healthy references without
unsafe-window calibration writes. NC requires pre-decode port-attempt/equivalent
complete coverage, ND real IDENTIFY activity, LK real reads plus a separate healthy
write/flush reference. Media equality is N/A for an unattached disk, never proof.
HP02 and HN37–38 exercise the causal distinction in data controls only.

**Remaining uncertainty:** exact NC/ND launch/topology/port coverage, future W site
registry and capacity, live calibration, locked-copy hash and CMOS model proof
are **UNRESOLVED U-DEVICE/U-WRITER/U-BUILD/U-CMOS**. No no-call runtime result is
claimed. **Disposition: PARTIALLY RESOLVED.**

## F06 — Replace ranges with single semantic boundaries

**Review issue:** pre/post ESP intervals contain different partial states; READ
submission, data readiness/transfer, wrapper return and backend pending are not
interchangeable. P3 cannot have a retrospective delta ledger.

**Source/compiled evidence:** P common pre-MOV `0x001000ea`, postalignment/pre-CALL
`0x001000f2`; primary capture memcpy CALL `0x001037b6`, cleanup `0x001037bb`, magic
store `0x001037be`; READ command OUT `0x00101daa`, next PC `0x00101dab`, first INW
`0x00101de0`. `ata.c:196–213,246–253` covers software PIO/retry/depth, not guaranteed
backend outstanding state. `emergency.c:45–58` makes active I/O also deny storage.

**Normative repair:** main section6/mutation M1–M3/oracle O1 define two independent
R2 IDs at those exact semantic points, with entry100 still partial and primary0,
predicted pre/post current stack and terminal ESP−12. R3 is complete body +magic0
at primary's pre-store boundary, not a generic memcpy hit. P2-READ-CMD-UD is the
first command submitted before any post-command guest poll/INW, count1/words0,
real depth1, owned operation/buffer; exact required denial is0x30. Backend read
may already be complete. P2-READ-DRQ0-UD and transfer/wrapper/backend alternatives
are distinct deferred designs. P3 has no current write delta permission.

Future numeric hook bindings and read model/status evidence remain **UNRESOLVED
U-BUILD/U-P2**. Resolving range wording does not make later device cases executable.
HP04–06/08 and HN35–36/39–40/46 separate these phases.
**Disposition: RESOLVED IN DESIGN.**

## Overall review request and preservation

The combined first tranche is withdrawn in favor of the separate gates in
`STAGE2B_AUTHORIZATION_LADDER.md`. H0-Core's finite textual catalogue and the
single corrected A1 candidate are ready for **independent design review**.
Open U items block affected implementation/calibration/execution transitions;
they are not filled by confidence or historical passes. No case is accepted here.

All deferred risks and bounded future AI/native-execution philosophy remain in
main section8. No production/host-test/observer/source file or original report was
changed. Final content comparison is reported separately in preservation/provenance.

**REVISION READY FOR INDEPENDENT REVIEW**
