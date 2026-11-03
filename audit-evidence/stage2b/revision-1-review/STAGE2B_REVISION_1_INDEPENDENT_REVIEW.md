# Stage 2B Design Revision 1: Adversarial Re-Review

Date: 2026-10-29, Asia/Dhaka. Scope: design, existing source, machine code and
retained evidence only. No experiment or host control was executed.

## Decisions

| Decision | Verdict | Exact scope |
|---|---|---|
| H0-Core | **READY FOR OWNER AUTHORIZATION CONSIDERATION** | The next request may cover H0-I: isolated, pure host-data implementation of the 56-row catalogue. This does not establish H0-C execution readiness or permit controls to run. Concrete input bytes and observer review precede a separate H0-C decision. |
| A1-#UD | **REVISE BEFORE AUTHORIZATION** | Not ready for an A1-E fault-execution request. The mechanism and semantic predicate are corrected, but exact build/RAM boundaries and operational CMOS/writer observations are missing. A separately scoped A1-I preparation proposal is distinct from permission to run it. |
| Global Stage 2B revision | **REVISE BEFORE AUTHORIZATION** | Do not submit H0+A1, or A1+P1+R1, as a combined execution tranche. |

The revision makes real progress on the previous review. F01, F03 and F06 are
resolved as semantic design findings; F02, F04 and F05 remain partially resolved.
This is not an acceptance of any new hardware result. If the owner's question
is immediate execution of either tranche, neither has completed its implementation/
observer/calibration gates. H0's positive decision is explicitly limited to the
next host-only implementation-authorization consideration in the revised ladder.

## Review Identity And Independence

Production HEAD is exactly `dbf23ce74a2a7103ae6fce04fba51969b7807178`, tree
`62ff1f4fbd5d70163c074fb7c259f4f2115c4d77`, clean branch
`feature/v2.6.2-memory-protection-phase-b`. The production ELF hashes to
`7cc1262fb1caee2b104d17d601a19e172eb02eac6d93b56ad2b2612a228db7bc`.

Reviewed directory:
`${LOCAL_EVIDENCE}\kmz-v262-stage2b-design-revision-1-dbf23ce`.
All 21 entries in its artifact manifest match their actual bytes. The manifest
itself hashes to `9c3ebe13c108e0f837005cb2a34a7e3b3a8efd4206defdd1edb043a9a17c396b`.
The seven referenced original design/review inputs and all five ELF identities
match their declared hashes. No input was silently substituted.

This is a new source/evidence assessment of the Stage 2B revision, not reliance
on its disposition labels. It is an AI review in the existing conversation,
which contains earlier implementation work on older kernel hardening. It is
not an authenticated external human audit or a fresh-context independent-agent
review. No previous authorship of the Stage 2B revision is claimed. The earlier
Stage 2B reviewer is a retained evidence source, not this review's authority.

## Findings Affecting The Next Decision

### F02: exact A1 mutation ownership is not yet established

M0-M6 now separates hardware frames, entry/capture state, diagnostic stack,
fixture bytes, architectural effects and unrelated memory. It correctly starts
the strict A1 comparison after CLI/pivot and immediately before UD2. Production
assembly confirms no old-stack software push or call before the switch.

However, the 4096-byte fatal-stack budget is a proposed cap. There is no future
fixture ELF with a complete stack/store witness, concrete arm/ack allocation,
complete normal-RAM capture domain or enumerated per-word PDE/PTE/GDT masks.
Those cannot be inferred from the production ELF or the existence of a 16-KiB
payload. A permissive implementation could hide corruption in an unqualified
stack/table mask. The revision explicitly prohibits that; the qualification
evidence nevertheless remains absent. A1 execution stays blocked by U-BUILD/U-RAM.

### F04: CMOS-T1 is a measurement design, not calibrated coverage

The source really writes nine cells 0x60-0x68 through ports 0x70/0x71, before
its conditional disk save. The proposed known-overwrite plus continuous trace
and audited RTC model chain is logically relevant. It excludes guest observer
reads, disk-as-CMOS inference and same-value-write false positives.

Both retained accepted traces reconstruct `4b 5a 01 00 01 00 00 00 59` before
activation and contain no recorded post-window RTC writes. This corroborates
the limited historical data claim. It does not supply source-to-binary RTC
coverage, exhaustive cell-update-path analysis or a continuously lossless
future-run sensor witness. A silent sensor satisfies a zero-event count too.
U-CMOS therefore remains open; reconstructed equality is UNVERIFIED for A1/P1/R1.

### F05: no-writer distinction is specified but not implemented/calibrated

The W contract now rejects entry to a forbidden writer even when a lower lock
returns before ATA I/O. This matches the actual store/journal/ATA early exits.
NC, ND and LK are separated, with sensor applicability rules and a pinned LK
seed. The source supports the predicted locked-store state without flag forgery.

No exact future-build forbidden-site registry, <=16 simultaneous-site capacity
witness, complete inline/indirect-edge coverage or positive entry-sensor
calibration exists. NC/ND topology/port-attempt evidence is also absent. Zero ATA
events cannot close these gaps. U-WRITER/U-DEVICE stay open. This blocks P1 and
the stronger no-call claims required by the new guest cases, not H0 data logic.

### Preservation qualification: historical completeness cannot be repeated as stated

All 6507 enumerated production/evidence files match, with no additions. Of the
7343 historical paths, 7279 exist and match; 64 are absent, all recorded as
zero-byte files. There are no changed surviving historical files or additions.
The missing paths are confined to Phase A, Phase B review and Stage 1 trees.
Original failed Stage 2A, revalidation and accepted Stage 2A trees match fully.

Author before/after inventories are byte-consistent, but this is not proof that
all historical paths still exist today. Two current ref-set differences concern
only `refs/codex/turn-diffs/...`; branch and tag refs match. Their causes/timing
are not established. Do not attribute them to the revision author or silently
restore files/refs. See the preservation audit and machine-readable receipts.

## Source And Machine-Code Conclusions

- A1: B=0x0014e000, T=0x00152000, S=B+1032=0x0014e408. Frame ESP is
  B+1020=0x0014e3fc; `[0x0014e3fc,0x0014e408)` lies entirely in the payload.
  The decoder accepts a bounded 12-byte same-CPL frame; CS08 gives origin_valid1.
  Policy compares headroom with 0x400, so 1020 produces BAD_STACK0x04 while the
  hardware frame can be perfectly healthy. The later software pivot does not
  change that captured origin. It proves neither overflow nor active guarding.
- R1: ordinary capture CALL=0x00102e58, emitter CALL=0x00102e6e, its return
  address=0x00102e73, authorization store=0x00102e8a. Emitter entry=0x001037e0,
  primary=0x0013dbc0, console sink=0x00102f20. A published eligible parent with
  authorization0 before emission is a real distinct state. C-SEC itself is absent.
- R2: pre-MOV ESP at 0x001000ea and post-alignment/pre-CALL at 0x001000f2
  are distinct exact production reference boundaries. R3 pre-magic is
  0x001037be after memcpy and cleanup, not a generic memcpy breakpoint.
- P2: READ OUT=0x00101daa, next instruction=0x00101dab, first INW=0x00101de0.
  Post-command/zero guest words does not prove backend pending. Active I/O makes
  both storage_ready false and io_active true: expected unsafe mask0x30 is sound.

These bindings were derived from the existing ELF independently of the author's
disassembly. They are reference facts, not future hook addresses or new execution.

## Oracle And Authorization Assessment

H0 comprises HP01-HP08 and HN01-HN48. All 56 are in H0-Core as interpretation/
retained-reference controls, including later-case tuples; none authorizes those
later guest cases. HP07 uses historical D evidence, not a new task-gate run.
Actual fixture bytes, mock identities, parser schema and per-row subvariants
must be frozen at H0 implementation review before H0-C can be considered.

The revised ladder separates design, implementation, build/observer review,
calibration, case execution and result review. It correctly makes UNVERIFIED
stop progression and forbids changing masks/checkpoints to rescue failed runs.
Missing evidence or a debugger stop cannot become PASS. Deadline failure with
working coverage and missing-observer timeout should be reported separately.

R1 no longer requires prior R2 execution. Its own eligible parent, actual caller,
raw secondary frame and qualified current fatal-stack observations establish
the intended late case. R1 and all three P1 profiles remain separately gated.
P2/U/R-D/P3 deferrals are preserved. The revision does not imply a combined grant.

## Stage 2A And Future Architecture

Retained Stage 2A acceptance remains limited to its recorded PF-during-PF
task-gate experiment. The failed original result remains false/exit1 at its
original digest. Two accepted saved cases retain TR0x30, live NT1, expected
hardware TS, HLT1 and equal before/after images. Their 62+4 assertion/66-case
claims are historical records, not tests rerun in this review.

No AI/END service is required in the emergency source graph. QEMU remains a
testbench; the design adds no mandatory application virtualization, unrestricted
mutation or removal of process isolation. Current global latch/records/TSS/stack
ownership is single-CPU and is not future SMP/process fault isolation. Versioned
future user-process/profiling/optimization contracts remain possible without
weakening these kernel emergency profiles. The END Engine was not inspected,
imported or modified for this review.

## Execution And Preservation Boundary

Builds0; tests0; host-control executions0; QEMU launches0; injections0; new fault
fixtures0; production mutations0; reviewer-issued Git writes0. Tools performed
file hashing, JSON/data inspection, read-only Git and static ELF disassembly.
The new inspection script is a receipt generator, not a host oracle/control
implementation. No historical build/test/observer script was executed.

The reports and supporting receipts are isolated outside production. The current
review cannot prove the absence of every unlogged historical host process. Its
zero-execution count describes actions performed for this review.
