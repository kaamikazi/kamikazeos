# CMOS-T1: Dual-Surface Calibration And Coverage Contract

**PROPOSED, NOT IMPLEMENTED OR CALIBRATED. RECONSTRUCTED EQUALITY UNVERIFIED.**
No QEMU/calibration, guest CMOS read/write or synthetic control is executed here.
Existing Stage 2A events are historical data only.

## C1. Required Observation Surfaces

Use two explicitly bound host observation surfaces for the exact emulator build:

1. **PORT:** CPU I/O attempt observation before device decode, reporting direction,
   width, raw port, value, retired/attempted instruction PC, CPU ID, sequence and
   model time. It must cover OUT at both0x70/0x71, even if a device callback is
   absent, aliased or rejected. It must not be a guest instrumentation log.
2. **CELL:** Effective RTC device cell-write/update observation at the actual
   update path, reporting selected/effective index, value, origin class, old/new
   cell value or update semantics, sequence and correlated PORT event ID.
   Model-internal update paths must be separately covered or proved unable to
   affect0x60-0x68. An event label alone is not coverage proof.

The retained event `mc146818_rtc_ioport_write` shows effective indices such as
0x60. Its old trace does not establish raw port0x70 selection, predecode attempts,
same-value coverage, all model updates or continuous losslessness. No claim is
made that existing QEMU exposes every required field. If either surface or the
needed correlation is unavailable, U-CMOS stays OPEN. A new instrumentation/model
change would require its own preparation authorization and source/binary review;
this document grants none.

## C2. Exact Coverage Matrix Required Before Calibration

For every row, the future receipt must contain actual kernel F instruction PCs/
bytes, device frontend/backend source symbols/line hashes and update edges,
actual event/counter site, build/object/debug-symbol/source correspondence and
classification DIRECT / EQUIVALENT / EXCLUDED-BY-PROOF / UNVERIFIED. Direct and
equivalent claims require independent review. A digest of source without a
credible source-to-executed-binary linkage is UNVERIFIED.

| Target | Current known kernel path | Future PORT/CELL binding | Present classification |
|---|---|---|---|
| port0x70 | cmos_write selects index via OUTB, including NMI mask bit semantics. | Exact predecode OUT site, device select/index mask/bank latch update and correlation. | Kernel source known; future dual-surface coverage UNVERIFIED. |
| port0x71 | cmos_write issues data OUTB after io_wait. | Exact predecode OUT and effective cell store; attempted write observed even if value unchanged. | Kernel source known; future dual-surface coverage UNVERIFIED. |
| cell0x60 | block[0], magic0x4b via save loop/cmos_write. | Effective index0x60 update edge and corresponding selected-index/data pair. | Future coverage UNVERIFIED. |
| cell0x61 | block[1], magic0x5a. | Effective index0x61, same obligations. | UNVERIFIED. |
| cell0x62 | block[2], version0x01. | Effective index0x62, same obligations. | UNVERIFIED. |
| cell0x63 | derived flags byte. | Effective index0x63, all flag values handled by same audited semantics. | UNVERIFIED. |
| cell0x64 | boot_count byte. | Effective index0x64. | UNVERIFIED. |
| cell0x65 | last_action byte. | Effective index0x65. | UNVERIFIED. |
| cell0x66 | prior_network byte. | Effective index0x66. | UNVERIFIED. |
| cell0x67 | prior_audio byte. | Effective index0x67. | UNVERIFIED. |
| cell0x68 | additive checksum byte. | Effective index0x68. | UNVERIFIED. |

The matrix must additionally enumerate guest callback aliases/banks, reset/
initialization, periodic timers/clock updates, device management, migration/load,
debugger/monitor and other host update APIs. For each path that can reach a
recovery cell, either instrument it with origin or prove it impossible in the
frozen run configuration. Initialization before a full known overwrite is
accounted separately; reset/load after it is forbidden. Clock/status updates
0x00-0x0d are not silently classified as recovery-cell equality. A hidden update
path leaves equality UNVERIFIED even if PORT observes zero writes.

## C3. Positive Calibration Workload, Still Separately Unauthorized

Calibration ID **CAL-CMOS-BOOT2**, on a separately owned healthy disposable image
and fresh VM, exact F unarmed, same CPU/RTC/observer/model identity as proposed
A1. It uses existing normal boot writes, not a debugger-invoked CMOS function:

- Select a frozen fresh healthy input with invalid initial RecoveryMemory CMOS,
  no scheduled safe mode/quarantines/panic/action, and expected count1. Pin the
  input image, firmware, topology and known initial/boot state. Do not assume
  fresh CMOS validity: capture the actual original guest reads and correlate
  them with the selected model/state before accepting this profile.
- Identify the first normal recovery_memory_init save and the later normal
  recovery_memory_mark_clean_boot save, by exact F call/return/data-flow/PORT
  evidence. Require independently captured live mem fields to be equal for
  these two saves in the chosen profile. If another legitimate write occurs
  between them, preserve it and account for it; do not cherry-pick nine events.
- For this fixed profile, both expected blocks are
  `4b 5a 01 00 01 00 00 00 59`, indices0x60..0x68 in that order. The second
  full save writes the same values and MUST still be detected as nine writes.
  If actual state differs, this calibration instance fails its selected input
  contract; do not change expected values after observing it.
- Every save requires nine raw port0x70 selects and nine port0x71 data attempts,
  matched to nine CELL updates at the same effective indices/values. Two saves
  therefore require at least those exact18 data updates and18 index selects,
  with any additional boot I/O separately inventoried. Port0x80 wait activity
  is neither a cell write nor missing port0x71 coverage.

This positive workload exercises all nine cells and idempotent writes without
adding a guest observer or changing the A1 unsafe window. It does not prove
every arbitrary model path/value by enumeration. The C2 static path/equivalence
analysis supplies the rest, or the coverage remains UNVERIFIED. A calibration
that observes only a subset of cells cannot qualify the missing ones.

No write calibration is allowed inside the A1 fault instance after arm/fault.
No new fixture or calibration input is produced in Revision2.

## C4. Causality, Ordering And Loss Accounting

Pin a common sequence/time mapping between CPU PORT attempts and CELL updates;
host wall-clock timestamps alone cannot order concurrent/queued events. Every
data attempt must map to an effective selected index and a classified update
outcome, and no CELL update may lack an origin. Match input/index masks, aliases,
width and value semantics to audited actual code. Record index latch/NMI state
without issuing an observer port select/read; inspecting the guest's mem struct
does not observe CMOS directly.

Require monotonic producer sequences, accepted/emitted/dropped event counters
at both surfaces and transport receive counts, a start boundary before boot
and independently observed flush/stop barriers through E-close. Counters and
heartbeat must be bound to the actual producer path, not merely the log writer.
A logger alive signal alone cannot qualify a disconnected RTC callback.
Pin immutable observer code/configuration; any reconfiguration, omitted buffer,
counter discontinuity, event truncation, cap overflow or unreviewed filter
invalidates coverage. A later matching reconstructed block cannot fill a gap.

The producer/counter/transport chain needs separately authorized positive and
loss/failure characterization. No calibrated loss witness is supplied now;
do not invent one from old events or assume all tracing backends are lossless.
Any mechanism requiring new model/observer code must be reviewed before its
calibration and must not be introduced as a hidden prerequisite implementation.

## C5. Unsafe-Window Acceptance And Silent-Sensor Rejection

After C2-C4 qualify exact identities, freeze the final complete known block at
E-boot/E-fault, including all intervening legitimate writes. From actual first
fault onset through E-close, require zero guest CMOS DATA writes to **any**
index, including idempotent and outside-block writes. Under qualified complete
PORT/CELL coverage and proof of no other recovery-cell changes, reconstructed
final0x60..0x68 equals initial. Label it reconstructed equality, not a direct
cell snapshot. Disk/RAM equality is no CMOS oracle.

| Evidence state | Required classification |
|---|---|
| Known calibration write missing with independently functioning capture path | Calibration FAIL; no valid certificate. |
| Calibration silent and capture/producer itself uncertain | UNVERIFIED; no valid certificate. |
| PORT sees a data attempt but CELL event missing/unclassified | Coverage FAIL or UNVERIFIED as supported by raw evidence; never qualified zero-write. |
| CELL changes without qualified PORT/model origin | Forbidden hidden update or unqualified coverage; never equality PASS. |
| Positive calibration valid but continuity/config/identity later missing | New instance UNVERIFIED. |
| Same-value CMOS DATA write during A1 | Immediate case FAIL even if reconstructed bytes unchanged. |
| All links qualified, no forbidden event, exact state reconstructed and all other A1 evidence valid | CMOS subclaim may PASS for that future instance only. |

Neither old calibration nor host mock agreement authorizes a run. Current
source-to-binary, exhaustive-update and positive/lossless future-run evidence
remain absent: **CMOS-T1 equality is UNVERIFIED, U-CMOS OPEN**.
