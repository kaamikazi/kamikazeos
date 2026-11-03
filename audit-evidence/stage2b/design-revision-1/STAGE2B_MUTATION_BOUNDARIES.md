# Stage 2B Revision 1 — mutation boundaries

**Design specification, not a measured write set.** No fixture, allocation, RAM
snapshot or device operation was created/executed here. All future-build numeric
allocations and exact store/stack witnesses are U-BUILD/U-RAM until reviewed.
See `STAGE2B_DESIGN_REVISION_1.md` for case IDs and P (reference ELF) identity.

## M0. Baseline epochs and comparison semantics

| Epoch | Required boundary / purpose |
| --- | --- |
| E0 | Fresh instance before arm: all crash slots, entry bytes, latch and counter zero; arm/ack zero. Never clear old evidence to manufacture E0. |
| E-boot | After normal clean-boot/journal/snapshot work has settled, actual shell readiness and depth0 proven. Save whole isolated image, recovery-state reconstruction and immutable boot trace prefix. Boot effects are accounted for separately. |
| E-live | At the last healthy-stack boundary before A1 pivot, after arm consumption and CLI. Record current ESP, true active-frame extent and old live-stack bytes; prove nonoverlap with the entire low target extent. |
| E-fault | Exact instruction boundary before the selected first UD2, after any fixture prefix/pivot, IF0. **Strict A1 whole-RAM/source delta baseline.** It is not a pre-keyboard/pre-confirm snapshot. Capture raw CPU, descriptors, maps, records and trace offsets. |
| E-parent | R1/R2/R3 exact pre-secondary UD2 boundary after declared hook ack, IF0. Save all128 parent bytes, entry100, auxiliary/counter, current stack/frame/CPU and original raw parent frame. This is the stricter secondary-preservation baseline. |
| E-term1 / E-term2 | Independently decoded terminal HLT, correct task/state, stable PC/ESP in observations at least250 ms apart; capture memory both times. Keep trace continuous through these stops. |
| E-close | Final guest stop/termination after evidence capture or deadline, final image readback and trace flush. Coverage extends to this boundary; host stop is not a replacement for HLT proof. |

Capture the ordered fault-onset marker from actual CPU exception evidence. Freeze
both its event offset and pre-instruction boundary; do not move the start past an
unwanted writer. For R1, writer prohibition begins at the **parent** fault, not
only E-parent. Parent processing before E-parent has its own declared mask;
E-parent adds a stronger byte-immutability rule for subsequent handling.

All masks are frozen in a reviewed, hashed per-build manifest before execution.
It must name exact half-open physical intervals, baseline hashes, field/bit
predicates, case and epoch, permitted writer/CPU ownership and maximum extent.
No overlap may confer a broader permission: hardware-frame bytes in the diagnostic
stack are M1, not an arbitrary M3 allowance. Any unidentified allocation or overlap
blocks the run. A changed mask requires a new design/build review and fresh case;
it cannot rescue an existing failed result.

RAM snapshot equality proves endpoint byte preservation in the declared normal-RAM
domain. It is not proof that no transient/idempotent store ever executed. Required
writer-entry/I/O absence therefore uses separate execution/request oracles. Reads
are host physical reads, not guest calls or guest MMIO probes. Enumerate ordinary
RAM and known MMIO/ROM separately; an unobserved RAM region makes full-domain
preservation UNVERIFIED. Do not hide it in an "other memory" exclusion.

## M1. Source and secondary hardware frames

### A1-UD-1020

B is bootstrap bottom. E-fault ESP=S=B+1032; only the 12-byte hardware frame
`[B+1020,B+1032)` may change in the entire bootstrap payload after E-fault:

| Offset from B | Width | Expected final bytes |
| --- | --- | --- |
| 1020 | 4 | LE address of the single future-linked UD2 |
| 1024 | 4 | LE kernel CS DWORD8 |
| 1028 | 4 | Raw saved EFLAGS DWORD, reconciled with pre-fault CPU and independently decoded fault semantics |

There is no same-CPL SS/ESP tail or error word for #UD. Match saved flags exactly
between raw frame, CPU delivery evidence and primary, with IF/NT/VM clear and the
architectural reserved bit present. The binding must handle the fault-delivery
RF semantics explicitly; no post-run arbitrary EFLAGS mask is permitted.

P illustration: `[0x0014e3fc,0x0014e408)`. The old live-frame extent from E-live,
all remaining bootstrap bytes and `[B-4096,B)` are immutable. Prove unchanged old
live-stack bytes at E-fault and terminal. No source-stack software write follows
the pivot; common entry changes ESP before its first CALL. The historical synthetic
frame.esp=CPU ESP−8 is only a value stored in M2, not permission for a write there.

### Other named cases

| Case | Hardware-frame permission relative to the strict baseline |
| --- | --- |
| P1 profiles / P2-READ-CMD-UD | One12-byte same-CPL frame at `[ESP_fault−12,ESP_fault)`, roomy boot stack; tuple comes from the exact linked trigger/driver checkpoint. No low-source pivot. |
| R1-UD-EMIT0 / R3-PRE-MAGIC-UD | After E-parent, exactly `[ESP_parent−12,ESP_parent)` in fatal payload; caller args/return address at and above ESP_parent are immutable. Original raw parent frame in bootstrap payload is also immutable. |
| R2-PRE-ESP-UD | At E-parent, ESP_parent is the original parent's hardware ESP. Secondary12 bytes immediately below it on bootstrap payload; original parent's12 bytes remain unchanged. Whole-case initial-to-terminal hardware extent is the two adjacent frames,24 bytes. |
| R2-POST-ESP-UD | At E-parent, ESP_parent=fatal top; secondary interval `[fatal_top−12,fatal_top)`. Original parent frame remains on bootstrap payload and immutable. |

Reentry assembly never changes ESP or makes a software push/C call. Its terminal
ESP must equal the secondary frame start, not a stack top. A third event, wrong
frame size/extent, source overlap or failing frame delivery is FAIL/stop, never
an enlarged hardware-frame mask.

## M2. Fixed handler-owned capture, latch and counter RAM

These are exact P reference spans; rebind by symbols/sizes/data flow for every
future ELF. End addresses are exclusive.

| Object | P physical span | Contract |
| --- | --- | --- |
| entry state | `0x0013db50..0x0013dbb4` (100) | First ordinary entry's assembly snapshot plus bounded decoder updates only; every field reconciled with CPU/raw frame. |
| primary | `0x0013dbc0..0x0013dc40` (128) | One body capture, reserved24 bytes zero, publication magic last; exact case fields, not unrestricted bytes. |
| DF slot | `0x0013dc40..0x0013dcc0` (128) | Zero and immutable for every selected normal-task candidate. |
| auxiliary | `0x0013dcc0..0x0013dd40` (128) | Zero for A1/P1/P2; exact sparse one-secondary record for R cases. |
| reentry count | `0x0013dd40..0x0013dd44` | Initial0; stays0 for O, transitions0→1 exactly once for R. No2/overflow/reset. |
| entry busy | `0x0013dd44..0x0013dd48` | First claim0→1; remains exactly1, including reentry's already-set atomic BTS. No clearing or high-bit changes. |
| ordinary_authorized | `0x0013dd48..0x0013dd4c` | Remains0 for these cases. O may execute the ordinary computed store of0; R1 is interrupted before the store. No eligibility repair/revocation edit. |
| runtime_ready | `0x0013dd4c..0x0013dd50` | Ready1 and immutable. Initialization is over. |

A1/P1/P2 first-entry allowance is at most100+128+4 bytes with changed values in
entry/primary/busy. Counter/DF/auxiliary must not change. An idempotent computed
authorization0 store is allowed only at its compiled ordinary-entry site.
Context/integrity getter outputs are stack locals in M3; they are not permission
to edit real readiness, RecoveryMemory or RecoveryStore globals.

R1 E-parent freezes **all128 primary +100 entry bytes**, published eligible1
included. R3 freezes them with primary magic0; copied bytes remain unpublished.
R2 freezes them at its earlier partial state: primary allzero; entry has complete
assembly stores but no C decode, frame offsets52..72 zero and frame.esp still the
hardware ESP. No R case can recapture, publish/repair the parent or modify those
100 entry bytes after E-parent.

For a single secondary #UD starting with a zero auxiliary, only these DWORD
offsets may become nonzero: 0=EM_MAGIC,4=version1,8=kind2,12=unsafe_flags0x40,
16=vector6,24=observed CR2,40=observed CR0,44=observed CR3. All other bytes remain
zero, including eligibility0 and absent EIP/frame-valid/stack details. The eight
listed DWORDs are at most32 changed bytes; counter adds4. Magic is published last.
This slot is partial secondary evidence, not a fabricated complete CPU frame.

The DF early-control/error globals are immutable in these selected profiles.
An unexpected update or task-gate entry fails; historical D does not widen M2.

## M3. Bounded emergency diagnostic-stack work

For first ordinary capture/emission, predeclare a **4096-byte design budget** at
the top of the fatal payload: `[fatal_top−4096,fatal_top)`, P illustration
`[0x0015d000,0x0015e000)`. This is a proposed maximum, **not a measured/proved
high-water mark**. All lower fatal bytes and reservations remain immutable.

U-RAM must be closed before execution by an independent exact-ELF stack/store
witness: every reachable push/CALL, prologue/local/spill, bounded loop, direct and
qualified indirect call, argument footprint, alignment effect and memcpy target.
It must prove maximum live depth and all possible stack-store intervals fit the
budget, including the complete hardware secondary frame at the chosen checkpoint.
If4096 is insufficient or the call/store graph is unresolved, stop and revise the
design; do not infer adequacy from the16-KiB payload or widen a mask after a run.

Only stack operations from that reviewed graph are allowed in the budget. It is
not an arbitrary-write exemption for bad pointers. Raw endpoint bytes, observed
ESP values and static store ownership must agree; snapshot-only observation does
not establish a universal write-attempt trace. Maximum distinct potentially
changed stack bytes is4096 minus overlapping M1/immutable intervals, not all kernel
RAM. No recursion, allocator, mapping change, error-log formatter or writer fits
this permission.

Epoch rules further tighten it:

- R1/R3 before E-parent may contain the parent's reviewed capture/call-stack work.
  **After E-parent no software stack byte may change**: only secondary M1 frame
  bytes may change. All earlier locals, return addresses and arguments are frozen.
- R2-PRE has no diagnostic software-stack allowance before or after its checkpoint.
- R2-POST has aligned ESP but the first CALL has not executed; the only new fatal
  bytes after E-parent are its12-byte M1 secondary frame.
- DF payload is unused and immutable in all these normal-task candidates.

This resolves the meaning of "outside the frame": source-stack bytes outside
M1 are immutable, while independently bounded normal capture/diagnostic work is
explicitly M2/M3. Exact future store coverage remains U-RAM, not a claimed result.

## M4. Fixture arm/ack and owned buffer

Only two new, separately allocated aligned DWORDs are proposed: arm and ack,
**8 bytes total**, disjoint from all production objects, stacks, tables and other
owners. Exact addresses are U-BUILD/U-RAM. Arm0/ack0 initially; explicit one-shot
arm0→1, then arm1→0/ack0→1. A1/P1/P2 consume before E-fault (A1 before pivot).
R1/R2/R3 retain arm1/ack0 at the parent fault and consume in the reviewed
nonspilling secondary-hook prefix before E-parent. That single predeclared
fixture-only transition is within the parent-fault window; after E-parent they
remain arm0/ack1. No later ack, retrigger, counter hidden in production state or
debugger metadata write is allowed.

P2 separately proposes one512-byte owned read buffer with exact future address,
initial-byte hash and nonoverlap proof. It is not authority to write another
buffer or sentinel sector. At READ-CMD no INW has executed; the buffer remains
byte-identical from pre-command through terminal. A completed host read cannot
DMA into this PIO buffer. Mid-transfer variants need another mask/review.

The manifest must also list any normal pre-fault command/rendezvous activity;
it is outside the strict window, never a retrospective mask within that window.
No fixture sets crash magic, readiness/lock/depth, parent bytes or architectural
metadata. No fixture or allocation has been created in this revision.

## M5. CPU architectural effects, not arbitrary metadata writes

Current O/R-N do not switch tasks. TR28/busy normal TSS stays; DF TSS descriptor,
backlink/error state and both TSS byte arrays remain unchanged from strict
baseline. Hardware task updates belong only to the unchanged historical D or
later independently reviewed R-D contract, not a normal-case exception.

Freeze exact walker/descriptor word locations before execution:

- Legacy PDE: only hardware accessed bit5 may transition0→1 for enumerated walks.
- Legacy nonlarge PTE: bit5 accessed and, only for enumerated actual write targets,
  bit6 dirty may transition0→1. Already-set bits cannot clear. No physical address,
  present/RW/USER/cache/global or other bit change is allowed; CR3 stays fixed.
- Kernel code/data descriptor accessed bit may transition0→1 only in the named
  descriptor's type/access byte if actually loaded. Base/limit/present/DPL/type
  semantics, task busy bits and unrelated descriptor bytes remain immutable.

These bit masks are **not yet enumerated for a future ELF (U-RAM)**. Do not exempt
entire directory/table/GDT pages. A software metadata store is not legitimized by
matching an allowed endpoint bit; the linked route must have no such store.
CPU registers may undergo the decoded delivery/entry instructions, including live
IF/NT and arithmetic-flag effects; require the exact profile, not arbitrary flags.
No SS reload occurs in common ordinary entry. No task return/IRET is allowed.

## M6. Immutable unrelated RAM and persistent state

After subtracting the narrowly declared M1–M5 spans/bits, every observed normal-RAM
byte is immutable: source/live-stack remainder, all10 inactive reservations,
DF/privilege/user stacks, kernel text/rodata, nonallowed BSS/data, heap/other owners,
page/table descriptor bits, RecoveryMemory flags and store/trust/driver state.
No shared "all .bss" or "entire diagnostic stack" mask is allowed. Per-profile
boot values, including P1 lock/refusal counters, freeze before fault.

Persistent rules run from actual first-fault onset through E-close:

| Profile | Permitted persistence delta/request |
| --- | --- |
| A1, R1/R2/R3, attached P1-LK | Zero new writer edges, zero new guest ATA commands/data writes/flushes, zero CMOS data writes; complete settled-baseline image and reconstructed recovery cells60..68 equal. Idempotent requests still fail. |
| P1-NC/ND | Same no-call/CMOS rules. No writable disk is attached to the driver's target; disk equality is N/A, not proof of media-write denial. Port/driver entry observations are still required. |
| P2-READ-CMD | Only completion of the one preowned READ may occur; no new command, guest data transfer/write/flush or CMOS writer. Full image and untouched PIO buffer equality; depth may remain1 in the abandoned caller. |
| Historical intentional panic | Its original authorized persistence ledger stays in its original control; it grants no unsafe-case writes. |
| P3 | DEFERRED. No sentinel-sector delta currently allowed; old/full/partial outcomes cannot be chosen after observing bytes. |

CMOS-T1 is PROPOSED—UNVERIFIED, not a direct snapshot; use the oracle chain and
U-CMOS gate. Clock/status evolution is separate; **all guest-initiated CMOS data
writes**, even outside60..68, fail the unsafe no-request window. No guest observer
index-select/read is introduced, so port70/NMI state is not changed by measurement.

All images for later authorized work must be fresh isolated copies/children;
never attach original production or accepted historical images. Keep the prepared
seed, settled-boot image, raw pre/post bytes, failed outcomes and any separate
next-boot copy. Host filesystem image equality is checked after orderly evidence
capture/flush; it does not replace request observations or CMOS state proof.
