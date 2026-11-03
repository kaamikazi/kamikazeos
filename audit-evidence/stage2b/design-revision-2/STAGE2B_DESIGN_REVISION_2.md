# KamikazeOS Stage 2B Design Revision 2

Date: 2026-10-29 (Asia/Dhaka). **DESIGN ONLY. REVISE BEFORE AUTHORIZATION.**
No implementation, fixture, build, calibration, host control or experiment is
authorized by this revision. All new runtime outcomes are UNVERIFIED / NOT RUN.

## 1. Baseline And Scope

Production remains clean at commit
`dbf23ce74a2a7103ae6fce04fba51969b7807178`, tree
`62ff1f4fbd5d70163c074fb7c259f4f2115c4d77`, branch
`feature/v2.6.2-memory-protection-phase-b` in `${REPOSITORY}`.
Existing production ELF SHA-256:
`7cc1262fb1caee2b104d17d601a19e172eb02eac6d93b56ad2b2612a228db7bc`.
This ELF is P, a reference, not a future fixture build F.

Authoritative inputs are Design Revision 1 and the seven re-review deliverables
in `stage2b-r1-review-20261029`. The re-review's global verdict and its distinction
between H0-I consideration and A1-E readiness are retained. F01/F03/F06 semantic
contracts, Stage 2A acceptance scope, the original failed Stage 2A result, later
R1/P1/P2/U/P3 gates and the single-CPU boundary are not weakened.

This revision addresses F02/F04/F05 only. The proposed A1 remains
**A1-UD-1020**: same-CPL #UD, initial ESP S=B+1032, hardware ESP B+1020,
frame12 wholly inside bootstrap payload, valid frame/origin1, published primary,
only BAD_STACK0x04 and persistence_eligible0, all other admission conditions true.
It is not overflow, recursive recovery or active guard-page enforcement.

## 2. New Closure Structure

The prior design named categories and measurement ideas. Revision 2 turns their
closure into three explicit, mutually bound future evidence bundles:

| Bundle | Required contents | Cannot substitute |
|---|---|---|
| B-RAM | Exact F/observer/launch identities, exhaustive normal-RAM domain, one owner per byte lane of every DWORD, per-PC store rules and finite union/high-water witness, phase-specific captures and attributed write stream. | A 4096-byte stack window, payload size, snapshot equality alone, or P addresses copied into F. |
| C-T1 | Exact RTC binary/source coverage table, independent predecode-port and effective-cell-update observations, known two-save positive calibration, idempotent detection, loss counters and continuous sealed window. | Old boot traces, RAM/disk equality, guest CMOS reads or a trace enabled flag. |
| W-ENTRY | Exact F forbidden-entry/inline/call-edge registry, complete reachable CFG and target witnesses, simultaneous capacity, per-site positive coverage and independent port/device activity. | Zero ATA events, writer guards returning early, a debugger STOP or a synthetic tuple. |

Every bundle has a single identity digest binding F, the exact emulator/observer
build, CPU/topology and its case. A receipt for another build or model cannot
close a missing field. No generated bundle, fixture state or synthetic control
input is created in this revision. Proposed field schemas below are documents,
not instantiated fixtures.

## 3. Exactness And Present Limits

All addresses in a future accepted bundle must be numeric and backed by the
actual linked F. Symbolic names below define how to obtain them; they are not
proof the object exists. Unknown numeric address, set cardinality, stack bound,
event coverage or calibration outcome is an explicit UNVERIFIED blocker.
There is no fallback which chooses a looser mask, smaller claim or different
trigger after observing a result.

| Finding | Revision 2 disposition | Exact remaining closure prerequisite |
|---|---|---|
| F02 | **PARTIALLY RESOLVED; U-BUILD/U-RAM OPEN** | Independently reviewed actual F with complete B-RAM word/lane ownership, store/stack union and qualified write attribution. No numeric future footprint is claimed. |
| F04 | **PARTIALLY RESOLVED; CMOS-T1 UNVERIFIED** | Complete C-T1 source-to-binary/update-path coverage and separately authorized positive/continuity calibration for the exact observer/model. |
| F05 | **UNRESOLVED: OPERATIONAL WRITER COVERAGE** | Complete W-ENTRY registry/capacity and positive site calibration, plus applicable NC/ND/LK device evidence. No claim of complete coverage is made now. |

The smaller uncertainty is now the absence of specific version-bound artifacts,
not discretion to decide what a good-looking trace means. The actual artifacts
cannot be supplied without later preparation/calibration authority; this revision
does not attempt to satisfy that by assertion.

## 4. Outcome Rules

PASS requires every required bundle present, matching and independently reviewed,
all exact A1 predicates true, no forbidden store/entry/request, complete raw
coverage and correct stable unsafe O halt. An observed invariant contradiction
is FAIL. Missing identity, coverage, attribution, baseline or complete observation
is UNVERIFIED. Both stop progression. Evidence is preserved without repair.

A silent positive sensor is FAIL as a calibration outcome, or UNVERIFIED if its
own capture failed. Either invalidates its coverage certificate. A zero-event
unsafe window with no current valid certificate is UNVERIFIED, never PASS.

## 5. Authority And Preservation

H0-I remains the only ready-for-owner-consideration host-only scope. The 56-row
catalogue is not run or expanded into a device/fault calibration tranche here.
A1-I preparation, each calibration and A1-E are separate decisions. P1 and R1
remain later separate proposals. A1 authorization status remains **BLOCKED**.

The latest review's qualified preservation result is retained: 6507 production/
evidence paths matching; 7279/7343 historical paths present/matching; 64 absent
paths originally recorded empty; branch/tag refs matching; two then-current
Codex-internal ref differences with unknown cause/timing. No missing file or
ref is restored to force equality. Any further current namespace drift is
reported separately rather than attributed to the revision author.

All execution counts for this task are zero. Read-only source/Git/symbol/hash
inspection and writing new design/provenance documents are the task's entire
action scope. No END Engine, AI emergency path, Ring 3, protection activation,
dynamic optimization, kernel-policy change or production alteration is included.
