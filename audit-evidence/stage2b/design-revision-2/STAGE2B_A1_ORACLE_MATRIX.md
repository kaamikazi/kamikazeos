# A1 Oracle Matrix: Explicit Evidence And Empty-Sensor Rejection

All rows are proposed acceptance predicates. **No row was executed.** PASS for
a prerequisite review is not experiment PASS or permission for a later gate.
Every required row is conjunctive: no majority, model agreement or selected
subset can compensate for a missing or contradicted invariant.

## O1. Case And Sensor Evidence

| ID | What / where / when observed | Qualification and allowed effects | FAIL | UNVERIFIED / silent-observer barrier |
|---|---|---|---|---|
| A-ID | Actual F source/tool/ELF/observer/model/launch identity at build review and before instance. | Independent byte/hash/disassembly review of numeric binding bundle; no production write. | Wrong identity/opcode/layout/policy or alternate trigger. | Missing F, unknown symbol/target or stale certificate blocks; P hashes cannot fill F. |
| A-ARM | Two F arm/ack DWORDs and control-flow path, E0 to E-live/E-fault. | Exactly reviewed allocation8, arm0->1 then consume1->0/ack0->1 before pivot. | Other owner touched, repeated arm, post-pivot write or forced readiness. | Unknown allocation/path/one-shot observation, unqualified stop; defaults cannot count as proof. |
| A-LIVE | CPU/L/T and complete bootstrap/normal RAM at E-live and E-fault. | L>=B+4096, true liveframe included in[L,T), nonoverlap with low target; register-only pivot, snapshots unchanged before fault. | Known overlap, source spill/call/push or wrong S. | Incomplete domain, unknown liveextent, post-keyboard/pre-CLI substitution or contaminated capture. |
| A-FAULT | CPU guest-origin vector6 at actual a1_ud2 and raw12-byte frame. | Qualify CPU trace/site/frame producer and exact pinned fault semantics. Hardware H only. | #DF/reentry/reset/#BP/debugger-injected event, wrong saved site/width/flags. | Breakpoint-only/empty trace/missing raw frame or unknown frame semantics never PASS. |
| A-CAPTURE | Fixed E100/C128, raw CPU/source frame and publication order during handling/terminal. | Per-word/PC/value rules; magic last, reserved0, validity independent of eligibility. | Frame/origin0, unpublished, wrong fields or recapture. | Serial text alone, absent frame/record event/order evidence. |
| A-CAUSE | Actual context readiness/trust/depth/root/text and primary fields at fault/capture. | Other admission predicates true; unsafe exactly0x04/eligible0, frame_valid/origin_valid1. | Extra denial flag, healthy headroom, wrong root/task or storage unavailable. | Unobserved real trust/depth or fixture-forged duplicate locals cannot establish cause. |
| A-RAM | Whole normal-RAM endpoints and all writes E-fault..E-close. | Complete qualified origin-tagged stream; H/Entry/Primary/Busy/Auth/S/A exact union, values and occurrence bounds. | Store outside owned rule, wrong value/origin/count, unrelated RAM change; no broad stack/table masks. | Missing wordowner, |W|/D, origin class, sequence or sensor positive coverage. Empty stream is insufficient when required capture writes should exist. |
| A-META | Actual PDE/PTE/GDT/TSS/root pre/post and architectural update stream. | Individually enumerated accessed/dirty changes0->1 for proven walks/writes; no task switch. | Software metadata store, semantic-bit/root/TSS/busy/backlink change. | Unknown word, hardware-update path not observed/qualified or arbitrary-page exemption. |
| A-CMOS | Matched PORT/CELL chain, known block E-boot/E-fault, window through close. | Current C-T1 positive idempotent/all-nine qualification, source-to-binary update coverage and lossless counters. | Any guest CMOS DATA write, even outsideblock/idempotent; forbidden other cell update. | Silent calibration, unqualified zero events, unknown update path or missing continuity makes equality UNVERIFIED. |
| A-WRITER | Complete numeric F registry and independent entry/edge events from firstfault to close. | Every occurrence covered simultaneously<=16 with per-site positive qualification and complete allowed-route proof. | Any forbidden entry even early-return/no ATA activity, ordinary formatter/delegation or unknown observed prohibited target. | Missing coverage/capacity/inline edge/positive receipt or cause of debuggerstop unknown. |
| A-DEVICE | Actual A1 healthy topology/trust, idle and drained request state at baseline; events through close. | No preowned pending write; no new ATA command/read/data/write/flush. Separate CPU attempts/device request counters. | New request or previously pending forbidden write completes after onset. | Depth0 alone without operation/queue ownership, missing positive sensors or absent callbacks. |
| A-HALT | Exact O unsafe-halt PC/HLT/IF/TR/CR3/ESP, two independently timed samples>=250ms. | F-specific source-dominated CLI/HLT/backedge, correct bounded fatal stack; no resume/IRET. | Dispatcher fallback, HLT0, moving ESP/PC, wrongtask/root, reset or continuation. | Paused-state/PC-only, <250ms interval or incomplete capture. |
| A-SERIAL | Actual full healthy-UART primary numeric block correlated with RAM/CPU. | Existing healthy serial contract and bounded actual emitter/sink. | Wrong record/text, unauthorized prefix/path or contradicted numeric field. | Missing/truncated output with unknown observer does not prove safehalt; do not relax healthy rules to match it. |
| A-PERSIST | Owned settled image before/after and C-T1 result plus W/PORT events. | Image unchanged; reconstructed cells unchanged only with qualified chain; no writer/request. | Known byte change or same-value forbidden request. | Equal image alone, unavailable CMOS, unflushed image/trace or missing baseline. |
| A-PRESERVE | Original source/images/reports, raw failure evidence and every currentbundle hash at close/review. | New directory/instance; first failure immutable; no automatic repeat. | Evidence overwritten/repaired, mask widened, wrong input/substitution. | Missing originals/logs/phase order or unknown causal mutation cannot become PASS. |

## O2. Profile-Specific P1 Checks Kept Separate

| Profile | Positive discrimination required | Why empty ATA is insufficient |
|---|---|---|
| NC | Exact no-controller topology, CPUff status beforeec, predecode port coverage and real unavailable RAMtuple. | No device callback exists to detect an attempted OUT; W entry and CMOS still matter. |
| ND | Controller present, realec IDENTIFY and status00, no targetdisk; qualified request observations. | Boot command activity proves controller only; writer entry can return without media. |
| LK | Actual later owned conflicting copy, preserved seedrecipe, genuine scan/order/lock tuple and read path. | Lowerguards suppress commands after a forbidden API call; W hit is immediate FAIL. |

P1 is not A1's storage baseline. A1 requires healthy storage so only headroom
denial can pass; unavailable/locked cannot mask a broken A1 headroom check.
No P1 profile is implemented, prepared, calibrated or authorized here.

## O3. Error Ownership And Stop Rules

- A known wrong field/store/site/request is an invariant FAIL, even if later
  bytes are restored or overall API status looks correct.
- A sensor known to miss its positive input fails calibration; its certificate
  is invalid. If whether the input reached the sensor is uncertain, classify
  UNVERIFIED and retain both possibilities. Never convert a quiet run to PASS.
- Missing/malformed input, parser/observer failure, trace loss and unknown
  intervention are distinct evidence defects. They are not proof of guest
  misbehavior or a successful negative control.
- Timeout with complete functioning coverage may FAIL a behavior deadline.
  Timeout with unknown coverage is UNVERIFIED regarding the guest. Neither
  permits progression, retrigger or a relaxed mask.
- Fail as soon as a forbidden entry/request is observed; preserve the first
  event and bounded best-effort state, then stop. Do not continue a suspect
  instance for calibration or to see if it writes later.

No new H0 rows are added or run in Revision2. The original56-row catalogue
continues to check interpretation separately; mock attestations cannot satisfy
A-RAM/A-CMOS/A-WRITER real sensor qualification. Its actual implementation and
input corpus remain subject to H0 observer review before an H0-C decision.

## O4. Current Outcome

The fixed case predicate is coherent. B-RAM, C-T1 and W-ENTRY operational
qualification remain **UNVERIFIED / NOT SUPPLIED**. The maximal footprint and
numeric registry cannot be filled without actual F. A1-E remains **BLOCKED**,
global verdict **REVISE BEFORE AUTHORIZATION**, with no execution authority.
