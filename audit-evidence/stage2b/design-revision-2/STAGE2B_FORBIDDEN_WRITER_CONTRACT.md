# W-ENTRY: Forbidden Entry, Call Edge And Device Contract

**F05 UNRESOLVED. No complete future registry or calibrated entry sensor exists.**
This is a contract for a future F-bound registry, not an active breakpoint list.
No observer, calibration workload, fixture, locked image or topology is created.

## W1. Core Registry And Expansion Rule

Every future registry row must contain semantic family/site ID, exact F PC and
instruction bytes/hash, function/inline origin and bounds, triggering call/branch
edges, direct/indirect/tail-call classification, caller/target qualification,
entry-observation point **before any guard/early return**, observation mode,
positive calibration receipt, case/epoch applicability and owning registry hash.
All executable occurrences count, not just the preferred external symbol.

| Family | Actual P reference entry | Relevant route / obligation in F |
|---|---|---|
| W01 interrupt_dispatch |0x00107a70|Ordinary exception delegation after authorization; any entry in unsafe A1/P1 window fails. IRQ invocations before onset are separate calibration activity. |
| W02 kpanic_exception |0x00111a60|Ordinary exception formatter; exact direct call and all clones/inlined bodies prohibited. A jump to reentry halt here is not an injected secondary fault. |
| W03 panic_ordinary |0x001118e0|Ordinary panic formatter branch; no unsafe-window entry. |
| W04 record_panic |0x00111a10|Private persistence gate/helper; include actual out-of-line body and every inline equivalent, not merely its symbol. |
| W05 recovery_memory_mark_panic |0x001133a0|RAM panic flag + save edge; entry fails before lower storage checks. |
| W06 recovery_memory_save |0x00112e20|CMOS block writes precede conditional disk persistence; every inline/clone entry/first-body edge covered. |
| W07 recovery_store_save |0x00115930|Can return at availability/lock/scan/order guards without ATA activity; entry itself is forbidden. |
| W08 recovery_journal_append |0x00115ed0|Can return when absent/locked/full; entry itself fails, including lock-counter increments. |
| W09 ata_read_sector |0x00101ce0|No new read API entry after A1/P1 onset. P2's separate preowned read is not an A1/P1 exception. |
| W10 ata_write_sector |0x00101e30|Entry fails before request validation, independent of command submission. |
| W11 write_sector_once |0x00101e90|Include direct/inline bodies and retry edges; no data write/flush is permissible. |

P entries are read-only symbol evidence, not future PCs. There are eleven core
families, **not proof that eleven observation sites suffice**. Expand a family
for each actual F clone/inlined first-body/call-edge occurrence. If one hook
purports to cover several paths, provide a sound complete dominating-edge proof;
a cold-symbol hit or inferred function name is not coverage.

All other possible normal writers must be in an exhaustive static inventory:
snapshot save, store clear, package manifest/state/slot writes, TinyFS writes/
format/delete and recovery-action mutators, including their inlines/indirect
edges. For this selected A1/P1 emergency route, prove each unreachable from the
full actual linked graph, with exact indirect sink target console_put and no
open edge. P examples include snapshot0x102080, slots0x10b0e0, package-state
0x10fb60, clear-store0x115c10 and TinyFS text-write0x129350. These examples are
not a complete writer inventory. Any reachable or unresolved writer must be
added to the simultaneous registry or leave coverage UNVERIFIED; no silent
exclusion because its source function normally returns or is uncalled by C.

## W2. Observation, Capacity And Integrity

Observe before first guard/body side effect, independently of guest logging and
device callbacks. Acceptable future implementation must positively qualify its
nonpatching guest instruction/entry observation, including zero-body early
returns and inline/tail/indirect paths. A debugger STOP without the exact
cause/site/coverage record is not a writer hit or proof of nonentry.

Bind an immutable sorted site registry and its source/CFG/ELF/data-flow hashes
to the observer. Require all required PCs simultaneously covered from the actual
first fault through E-close. The existing design cap remains **<=16 concurrent
distinct sites**. Include any needed boundary/sensor sites in a documented
capacity/resource allocation so overlapping hardware breakpoint uses do not
silently consume slots. Never assume unlimited Z1 capacity, rotate sites,
use software INT3 or replace missing coverage with polling.

If actual registry expansion or observer-resource demand exceeds proven
capacity, A1/P1 is blocked. A different complete observation backend or changed
budget needs new review/authorization, not an automatic fallback. Readback of
an armed-address list is not proof the execution path is connected to detection.
Actual entry-event producer sequences, loss counters and code/config identity
must remain qualified across the whole window, as in CMOS-T1.

Combine entry observation with a full selected-route static/actual control-flow
witness. A permitted indirect console sink cannot become arbitrary targets;
unknown branch/target/exception path is UNVERIFIED or observed forbidden flow
FAIL. Device PORT and command/data/flush observations remain independent vetoes
against unlisted bypasses. Neither source proof nor trace absence alone proves
all possible out-of-band host corruption is confined; that is outside scope.

## W3. Positive Entry Calibration, Separately Gated

Before an empty unsafe window can qualify, every row/site class must have a
positive actual-event receipt in the exact unarmed F with same CPU/accelerator/
observer backend. Match producer sequence/PC/bytes, caller/target and resulting
independent control-flow observation. Exercise an early-return class as well as
a body reaching I/O, to distinguish entry detection from device detection.

Candidate future calibration profiles, all requiring explicit separate owner
authority, are: ordinary healthy unarmed boot/read/state/journal/write/flush
activity; existing healthy-stack ordinary #UD persistence; and existing
healthy-stack panic persistence. The latter two are genuine fault/test
calibrations outside A1, and **are not authorized by A1-I or H0**. They must be
named separately with their own owned images, evidence and outcomes. No direct
debugger call into a writer or artificial writer inside A1/P1 is permitted.

If an inline/site cannot be exercised by a permitted healthy path, either prove
equivalence to a calibrated observation primitive with complete F path coverage
or keep it UNVERIFIED. Equivalence is not inferred from a nearby instruction
hit. If a genuine early-return calibration cannot be obtained without a new
fixture, stop for a separate proposal; do not claim it passed now.

A positive event must be detected even when the writer produces no ATA request.
Calibrate port/write/flush sensors separately on a healthy attached-media
reference. Bound source-to-PC/event coverage and transport loss detection;
simulated H0 entry tuples check interpretation only. Known entry with silent
sensor is calibration FAIL; uncertain capture is UNVERIFIED. Both block.

## W4. Distinct P1 Profiles And A1 Baseline

| Profile | Actual initialization and observations required | Unsafe-window result required |
|---|---|---|
| A1 | Healthy present disk/trusted store; runtime/DF readiness true, depth0 and complete settled boot. No pending write/flush; validate actual operation ownership and drained model queue, not depth alone. | No registry entry and no new ATA command/data/write/flush/CMOS write; exact headroom-only denial0x04. |
| NC | Exact topology has no primary ATA controller decoded at1f0..1f7/3f6. Observe real CPU statusff branch before IDENTIFY; present0/capacity0/depth0, valid0/locks0/scan0/order0/source none/generation0/trust0. | No forbidden entry or port attempt after fault. Device callback silence is inapplicable to an absent controller; predecode attempts or a qualified complete instruction/I/O equivalent are mandatory. Media equality N/A. CMOS still required. |
| ND | Primary controller decoded, no master disk. Observe actual ec IDENTIFY submission then status00 branch; same initialized unavailable tuple as NC but different topology/port evidence. Pin secondary/other routes so they cannot impersonate the target. | Same no-entry/no-new-command rule; observed IDENTIFY at boot is not calibration of later media writes. Media equality N/A. CMOS still required. |
| LK | Real2048-sector target and only separately prepared conflicting mirror on pinned owned seed; valid0/locks1/scan1/order1/source ambiguous/mirror1, RAM generation0/jcount3/trust0/depth0. Original CMOS invalid and readable journal floor1 imply diagnostic boot2. | Genuine lock path observed through actual F loads/branch/counter values; no writer call after onset even if it would return; no new ATA activity, including read. Full settled image and qualified CMOS equality. |

"LK without ATA activity" refers to its unsafe interval, not boot: normal
IDENTIFY/state/journal READs are necessary evidence the real locked path was
reached. Freeze the settled-boot window and request offsets before fault so
these reads cannot be moved across the boundary to conceal a handler request.
No initialization field may be forged by fixture/debugger to qualify a profile.

The original seed remains at its historical path, SHA-256
`2d1a74227f1ec230a60eb447d534ed5bd67866ee859ca377f64c549c3b3413d6`,
1048576 bytes, primary/mirror equal, boot1/generation5/jcount3. The later LK
proposal must separately authorize a new copy, mirror byte5=200, base checksum
byte17 reseal, CRC32 bytes48..51 over0..47 and no other changed byte. Record the
new copy hash before any boot. No copy or conflicting sector is made here.
No fresh invalid-CMOS assumption is accepted without original guest-read/model
evidence; no observer CMOS read is added to manufacture it.

NC/ND/LK each require same-class topology/sensor qualification. A healthy-media
calibration can prove entry PC/I/O-format detection but not absence of decode
in NC or exact no-disk behavior in ND. Any claim of topology-independent entry
coverage requires actual backend/source identity and reviewed equivalence.

## W5. Causal Categories, Immediate FAIL And Silent-Sensor Rules

| Observed state | Classification |
|---|---|
| Qualified complete registry coverage, no entries, no forbidden I/O, all other exact case predicates true | No-entry subclaim may PASS for that later instance. |
| Writer site entered and returned before ATA I/O | Immediate case FAIL; freeze the entry event/PC and evidence. Do not resume to see if it eventually writes. |
| ATA command/data/flush attempted, including same-payload/idempotent | Immediate FAIL even if image unchanged or API call not observed. |
| Previously submitted operation completes after onset | Correlate exact operation; A1/P1 must prove none pending at baseline. Unowned/pending write is FAIL or unknown causality UNVERIFIED; no blanket completion exception. |
| Required site absent/unbound, registry count/capacity incomplete, unknown inline/indirect path or lost event | UNVERIFIED; no A1/P1 trigger authorization. |
| Positive known entry not detected with functioning capture | Calibration FAIL; invalidate registry qualification. |
| Empty trace, debugger stop or timeout without current coverage/cause | UNVERIFIED, never PASS. |

No complete numeric F registry, <=16-site capacity receipt or positive/live
coverage exists now. NC/ND exact topology/port witnesses are absent. **F05
remains UNRESOLVED, U-WRITER/U-DEVICE OPEN.** A static example registry is not
reported as operational coverage.
