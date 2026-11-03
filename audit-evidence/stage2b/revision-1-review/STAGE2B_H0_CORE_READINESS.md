# H0-Core Authorization-Readiness Assessment

**READY FOR OWNER AUTHORIZATION CONSIDERATION**, limited to H0-I pure host-data
implementation. **No H0-C execution readiness or execution approval is granted.**
If a proposed owner request combines implementation and immediate controls,
revise it to preserve H0-I -> independent input/observer review -> H0-C.

## Exact Membership And Effects

All HP01-HP08 and HN01-HN48 belong to the first H0-Core specification: eight
positive and forty-eight negative rows. Their R/P2 labels select synthetic
interpretation profiles, not guest experiments. HP07 is retained Stage 2A D
reference interpretation. Production and accepted-ELF modes are read-only byte/
CFG inputs; mock UD2/checkpoint addresses must carry a distinct binding identity.

No row requires calling a guest function, arming a fixture, creating a recovery
disk, running the kernel, launching QEMU, selecting CMOS ports or accessing a
provider. A later host interpreter may read/hash existing artifacts and transform
explicit synthetic tuples. Real RTC/W/device calibration is excluded and remains
separately gated. No such interpreter/control fixture was created in this review.

The common template specifies source/fatal bounds, selectors, CR3, flags, raw
frame extents and evidence modes. Values are deterministic specifications but
not yet concrete serialized input files. Exact GPR/flags bytes, attestation
schema, trace offsets, per-row base positive and subvariants need frozen bytes/
hashes at H0 implementation review. Missing input is UNVERIFIED, not an implicit
default-positive. This is a gate for H0-C, not a demand to execute before design.

## Eight Positive Rows

| ID | Bounded accepted interpretation | Audit condition |
|---|---|---|
| HP01 | A1 raw12 at B+1020, S=B+1032, valid frame/origin1, only unsafe0x04/eligible0, O. | C decoding and policy support it. Synthetic classification only; does not prove CPU delivery. |
| HP02 | Mock real-profile LK state, only BAD_STORAGE, no writer/request, qualified mock CMOS chain. | Must treat declared sensor qualification as synthetic data; no real calibration claim. |
| HP03 | R1 parent eligible1/auth0, exact caller/destination, mock[6,6], preserved parent/entry, counter1 and R-N. | Non-vacuous parent/caller fields; missing raw secondary evidence cannot be repaired from the halt. |
| HP04 | R2 pre-MOV partial state; raw source-stack ESP minus12 terminal. | Distinguishes old bootstrap stack from fatal top; primary remains zero. |
| HP05 | R2 after alignment/pre-CALL; fatal top minus12 terminal. | Distinct boundary and expected partial fields; cannot accept post-CALL stack. |
| HP06 | R3 copied body/magic0 preservation, secondary auxiliary/R-N. | ACCEPT byte preservation only; semantic publication remains UNVERIFIED. A single Boolean PASS for both is wrong. |
| HP07 | Hash-bound retained healthy D under unchanged62+4/serial contract. | Historical reference result only. Existing complete accepted tree is available; some unrelated earlier zero-byte logs are missing. No rerun. |
| HP08 | Mock READ command0x20/count1/LBA2047/depth1/zero words and known model phase, unsafe0x30. | Backend may be complete; must not assert backend-pending from depth. |

## Forty-Eight Negative Rows

Each row below has an objective discrepancy. REJECT means contradicted input
contract, not a failed hardware experiment executed here. A row expecting
UNVERIFIED must reject certification without being mislabeled a proven guest bug.

| ID | Expected interpretation | What the future implementation must actually compare |
|---|---|---|
| HN01 | REJECT | A1 frame width16 versus12, not just published record flag. |
| HN02 | REJECT | frame_valid0 despite required1. |
| HN03 | REJECT | origin_valid0 despite CS/task/source contract. |
| HN04 | REJECT | unsafe0x14 versus exact0x04; storage failure must not mask headroom. |
| HN05 | REJECT | eligibility1 versus required0. |
| HN06 | REJECT | magic0 versus completed publication; body alone insufficient. |
| HN07 | REJECT | vector8/task-gate route versus required vector6 ordinary path. |
| HN08 | REJECT | Byte outside the exact source frame changed. |
| HN09 | REJECT | Old live-frame byte changed between the declared epochs. |
| HN10 | REJECT | Dispatcher fallback0x102eb2 versus unsafe O0x102ec2. |
| HN11 | REJECT | Correct PC with HLT0; pause is not halt. |
| HN12 | REJECT | O/R-N has TR0x30 instead of0x28; freeze chosen base-profile subvariant. |
| HN13 | REJECT | SS differs from0x10. |
| HN14 | REJECT | Actual CR3 differs from bound root. |
| HN15 | REJECT | R-N live NT1 instead of0; do not relax D globally. |
| HN16 | UNVERIFIED | 249 ms does not establish required250-ms observation span. |
| HN17 | REJECT | Stable-sample ESP actually moves. |
| HN18 | REJECT | Recorded reset/firmware/triple-fault after onset. |
| HN19 | UNVERIFIED | Missing CPU event stream; debugger stop alone. |
| HN20 | UNVERIFIED | Missing RTC stream/window. |
| HN21 | UNVERIFIED | Empty events without qualified positive/continuous coverage. |
| HN22 | REJECT | Same-value CMOS data write, despite equal final cell bytes. |
| HN23 | REJECT | Same-payload ATA write/flush, despite equal image hash. |
| HN24 | REJECT | R1 parent ineligible before secondary. |
| HN25 | REJECT | R1 authorization already1 at chosen preauthorization checkpoint. |
| HN26 | REJECT | DF record passed to generic emitter, not ordinary primary. |
| HN27 | REJECT | Primary pointer but wrong actual caller/return. |
| HN28 | UNVERIFIED | SIGTRAP/STOP without CPU secondary vector/site/frame. |
| HN29 | REJECT | Direct authorization-failure shortcut to reentry halt without intended causal route. |
| HN30 | REJECT | Reentry count2 instead of exactly1. |
| HN31 | REJECT | Count0 with claimed secondary capture. |
| HN32 | REJECT | Any changed R1 parent byte in128-byte snapshot. |
| HN33 | REJECT | Any changed R1 entry-state byte in100-byte snapshot. |
| HN34 | REJECT | Raw secondary saved EIP differs from selected actual UD2. |
| HN35 | REJECT | R3 magic becomes published/repaired after frozen magic0 snapshot. |
| HN36 | REJECT | R2-pre uses fatal-top-derived terminal ESP. |
| HN37 | REJECT | Writer API entered but lower guard returns, no ATA events. |
| HN38 | REJECT | P1 unavailable label contradicts independently healthy/trusted target. |
| HN39 | UNVERIFIED | Positive depth without actual submitted-read identity. |
| HN40 | REJECT | P2-CMD replaced by wrapper return after256 transferred words. |
| HN41 | UNVERIFIED | RTC model can alter cells without observed guest write. |
| HN42 | UNVERIFIED | Incomplete known overwrite or gap before strict baseline. |
| HN43 | UNVERIFIED | Trace dropped, truncated or cap exceeded; never silence-as-PASS. |
| HN44 | REJECT | Synthetic/retained input presented as fresh hardware evidence. |
| HN45 | REJECT | Parent output prefix exists before R1 first-emitter checkpoint. |
| HN46 | REJECT | Generic/mid-copy hit substituted for complete primary pre-magic boundary. |
| HN47 | UNVERIFIED / reject trigger | Prospective frame cannot be proven wholly contained. An observed out-of-bounds extent is also a contradicted preflight invariant; never trigger. |
| HN48 | REJECT | Store outside owned M2 or unapproved table-bit change; freeze each subvariant rather than broad-mask it. |

## Quality, Redundancy And Implementation Gate

The rows are partly overlapping intentionally: HN08/09 overlap byte preservation,
HN11/16/17 separate PC/running/stability, HN20/21/41/42/43 separate measurement
prerequisites, and HN24-29/45 separate R1 parent/caller/event phase. These are
useful independent failure modes, not 56 demonstrated mechanisms or proof of
exhaustive input coverage.

A malformed observer could still pass if it uses default true values for absent
fields, checks only magic or status text, ignores parse errors, treats an empty
negative corpus as success, trusts evidence_kind strings without bound mode,
or compares output against an expectation computed by the same implementation.
H0 implementation review must require independent fixed expected outcomes,
strict input/schema coverage, all56 row IDs, per-assertion failures and explicit
fixture/parser/binding failures. The catalogue does not yet contain standalone
negative rows for every malformed JSON/schema; these are implementation quality
obligations, not present control results. No absent/error fixture may count as a
successful negative rejection unless the case actually targeted that error.

H0-C must review the actual serialized controls, immutable reference manifests,
reproducible expected outcomes, bounded parser/CFG interpretation and execution
scope. HP06 needs a byte-only outcome; HP07 needs retained-reference provenance.
Mock attestations exercise interpretation, never a live RTC/breakpoint sensor.

The next H0-I request is sufficiently bounded because it has explicit membership,
per-row interpretation expectations, immutable reference identities, separate
synthetic/reference modes and no guest/device effects. This readiness does not
extend to actual fault behavior, W/CMOS calibration, H0-C, A1, P1 or R1.
