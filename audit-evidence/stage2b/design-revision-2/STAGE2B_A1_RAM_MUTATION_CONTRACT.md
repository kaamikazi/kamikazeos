# A1 B-RAM: Word Ownership And Attributed Store Contract

**No ownership map, write observer or fixture is implemented here. U-RAM is OPEN.**
Reference P addresses are illustrative bindings to the existing ELF. An accepted
future B-RAM bundle must instantiate all addresses/cardinalities from actual F,
not reuse P or treat an unproven symbol as permission to write.

## R1. Exhaustive Physical Domain

Freeze a complete topology inventory for the exact future machine before run:
every RAM-backed physical interval and alias, ROM, MMIO, absent hole and device
owner. Reconcile machine memory topology, launch, guest memory-map bytes, ELF
PT_LOAD/section bounds, actual root/PDE/PTEs and the normal-RAM capture backend.
The 16-MiB identity aperture is not the whole normal-RAM domain on a64-MiB guest.
RAM above the aperture remains immutable unless a specific owner/write is proven;
it cannot be discarded because current kernel pointers should not reach it.

Every physical DWORD intersecting normal RAM must have one owner and a four-bit
byte-lane map, including partial boundary words. Required row fields are:
PA, aliases/translation evidence, region ID, owner symbol/section or explicit
unallocated/firmware-RAM owner, lane mask, epoch baseline offset/hash, writer
origin class, allowed instruction PCs, effective-address rule, value/bit predicate,
occurrence bound, and immutable lanes. Overlapping owners are a blocking error;
page-table storage is never also general BSS allowance. Adjacent padding/holes
inside allocated RAM have explicit immutable owners. MMIO is not read by a RAM
snapshot routine and ROM is separately hashed, not silently included as RAM.

An unknown interval, alias, instruction writer, DMA/model writer or missing
snapshot segment leaves whole-domain preservation UNVERIFIED. A false topology
claim may be FAIL if contradicted by model/launch/map evidence. No allow-all
domain entry is admitted. A1 uses one CPU and a pinned model with no unaccounted
RAM DMA; lack of proof cannot be replaced by assuming IF0 excludes devices.

## R2. Capture Epochs And Original Stack

| Epoch | Required capture and timing |
|---|---|
| E0 | Fresh unarmed instance; exact F objects/records initialized, magic/latch/counter/arm/ack zero. Never clear evidence to fabricate it. |
| E-boot | All normal boot store/journal/snapshot work settled; actual idle ATA and trust/readiness observed; full owned image and calibrated CMOS starting state. |
| E-live | At exact `a1_live_before_pivot`, after one-shot consumption and CLI, before ESP changes. Capture CPU and all normal RAM. Let L=observed ESP, require L>=B+4096, L<T. Preserve live region `[L,T)` and whole bootstrap `[B,T)`. |
| E-fault | Exact `a1_ud2` boundary immediately after pivot, IF0, ESP=S=B+1032. Capture CPU/root/maps/records and complete normal RAM through a nonpatching observer. No source/software store may intervene between E-live's pivot boundary and UD2. |
| E-term1/2 | Actual unsafe O HLT with stable PC/ESP samples>=250ms apart, IF0/TR0x28/root correct. Capture the same complete RAM domain both times and complete raw write/event streams. |
| E-close | Final stopped window, loss/accounting barriers and orderly capture flush; full settled-image readback. Stop is not itself HLT proof. |

The true original live extent is witnessed by stack/call/unwind analysis at F's
boundary, not merely EBP. `[L,T)` is the exact conservative preservation interval
for it; additional stale stack bytes are also immutable. Require live stores and
reachable references disjoint from `[B,B+1032)`. No restore/return is attempted.
E-live and E-fault snapshots must already agree for every bootstrap byte; changing
ESP is register-only. Keyboard/PIT/normal arm work before CLI belongs before this
comparison. No later keyboard confirmation is used to define the strict baseline.

An observer stop must not patch guest bytes, set registers, inject #DB/#BP, call
guest functions, remap pages or modify CMOS/NMI state. An incomplete stopped-RAM
capture is UNVERIFIED. Unknown host/device contamination is not ignored or masked.

## R3. Hardware Frame Versus Software Records

The only source-stack write support after E-fault is exactly
H=`[B+1020,B+1032)`12 bytes. Its DWORD rows are:

| Address | Owner/value rule | Proven origin |
|---|---|---|
| B+1020 | Linked F `a1_ud2` EIP | CPU vector6 delivery; not a software copy. |
| B+1024 | Saved CS slot for0x08 under pinned CPU's exact frame semantics | CPU vector6 delivery. |
| B+1028 | Saved EFLAGS reconciled with preinstruction CPU and explicit RF/reserved-bit semantics | CPU vector6 delivery. |

Require raw frame, actual CPU exception/site and attributed hardware-frame write
events, not just a matching software record. No error word or user SS:ESP tail
exists for this same-CPL #UD. Upper CS-slot bytes and saved RF behavior must be
resolved from the pinned CPU/model contract before exact-byte acceptance; do
not widen a flag/slot mask after a discrepancy. Synthetic PUSHA ESP=CPU ESP-8
is only a field value stored elsewhere and permits no source write there.

Bootstrap `[B,B+1020)` and `[B+1032,T)`, including all original live frames, are
immutable from E-fault. Low guard reservation and page tables outside B stay
immutable except their individually qualified hardware bits. Common entry's
later software pivot to fatal_top does not change H or the original headroom.

## R4. Fixed Entry/Capture Objects: Per-DWORD Rules

Bind all object bases/sizes from F. P reference entry E=0x13db50, primary
C=0x13dbc0, DF=0x13dc40, auxiliary=0x13dcc0, count=0x13dd40,
busy=0x13dd44, authorization=0x13dd48, runtime=0x13dd4c.

Entry E has25 DWORD rows; every permitted write needs its exact source/PC:

| E offsets | Permitted sequence/value |
|---|---|
|0,4,8,12|Original GS/FS/ES/DS from E-fault CPU, zero-extended as exact machine instructions establish. |
|16,20,24,32,36,40,44|Original EDI/ESI/EBP/EBX/EDX/ECX/EAX; no arbitrary saved GPR values. |
|28|Initial CPU frame ESP during assembly, then decoder's CPU ESP-8 convention. |
|48|Vector6 from selected ISR, one first entry. |
|52|Assembly zero; decoder error0. |
|56,60,64|Assembly zero then independently bound raw EIP/CS/EFLAGS. |
|68,72|Zero, no privilege tail. |
|76|CPU ESP=B+1020. |
|80,84,88|Observed CR2/CR0/CR3 from first entry, linked to independent CPU snapshot; #UD gives no new CR2 cause. |
|92,96|has_error0 and kind1, from exact ISR stores. |

Primary C has32 DWORD rows. Every row is first copied from the bounded local
record n under the reviewed memcpy implementation, then only magic offset0
may publish EM_MAGIC0x454d4352 last. Its payload fields must agree with E/raw
CPU/context: version1/kind1/vector6/error0, source/origin/stack bounds, exact
unsafe0x04/eligible0, frame/origin/eflags_known1, cr2_applicable0/context1.
Diagnostic ESP must name the proven current capture-stack point. Reserved
offsets104..127 are zero. A field-copy PC with a wrong value is not allowed.

Busy has one first-entry bit0 acquisition0->1; other bits zero. Count, DF and
auxiliary remain entirely zero. Authorization may receive one computed store0
at F's exact ordinary-entry site, despite unchanged endpoint bytes; other stores
to that word fail. Runtime/readiness, early DF globals and all TSS/task bytes
remain unchanged. No error-log/allocator/ordinary formatter owner is writable.

## R5. Diagnostic Stack: Exact Store Support, Not A Region Mask

Let Fbottom/Ftop come from F's fatal-stack object and linker aliases, with their
agreement independently checked. Preserve the proposed4096 cap
`[Ftop-4096,Ftop)` only as a ceiling for a proof, never as an allowlist.

The future `A1_STACK_STORE_WITNESS` must enumerate every reachable instruction
from vector6 entry through denied O halt, including hardware source-frame stores,
CALL return addresses, prologues, dynamic alignment, saved registers, local
objects, spills, structure copies, loop bounds and exact indirect console sink.
Each stack write row contains instruction bytes/PC, concrete call context,
effective-address interval/expression, byte width/lanes, value provenance,
minimum/maximum occurrence count and pre/post ESP. Recursion or unresolved
indirect/call/stack-allocation behavior blocks the proof. Do not infer worst-case
depth or write support from a sampled successful run.

For bounded loops, expand their finite support or provide a reviewed closed-form
set with exact numeric cardinality and count bound. Different call paths must
remain conditional: their union is only a maximum support, not permission for
an impossible path or wrong value. A memcpy into primary has a separate C owner;
its source local belongs to a specific stack object, not arbitrary stack bytes.

Require an exact set S of possible software stack-store byte lanes and a proven
maximum live depth D. Both are numeric/canonical in the binding, S entirely within
the ceiling and disjoint from immutable owners. Every byte below the proven
support stays immutable. Any attempt to certify by `max_depth<=4096` without S,
or by accepting any store within S without its PC/context/value/count rule, fails
build review. A larger needed footprint requires a separately reviewed design;
it cannot be grown after a fault result.

## R6. Architectural Writes: Exact Word And Origin

For each actual walk/descriptor access in F, identify the physical DWORD and
the relevant byte lanes; record the baseline value and entry owner:

- PDE bit5 may transition0->1 only for an enumerated actual legacy walk.
- Nonlarge PTE bit5 may transition0->1; bit6 only for an enumerated actual
  permitted write target. Physical address/RW/USER/present/cache/global bits
  are immutable; bits already1 cannot clear.
- The named kernel descriptor accessed bit may set at its exact access byte
  on the demonstrated load; no semantic segment field changes.
- GDT TSS busy/backlink, normal/DF TSS, IDT, CR3/root/CR4 and CR0 protection
  policy remain unchanged in A1. No hardware task switch is allowed.

Before authorizing a run, enumerate every possible affected word from F's full
reachable fetch/read/write graph and initial map. A software store that happens
to set an allowed hardware bit still fails. Distinguish guest instructions,
exception-frame micro-operations, page-walker updates, descriptor accesses and
host/device writes in the observer's origin tags. Existing trace/event names
do not prove such origin coverage; an absent origin-qualified observer is U-RAM.

## R7. Maximum Footprint And Store Attribution

For strict E-fault..E-close, fixture delta/store allowance is zero. Define A as
the exact allowed architectural-write byte lanes, and S as above. The fixed
maximum distinct target support, counting idempotent writes, is
`W = H union Entry100 union Primary128 union Busy4 union Authorization4 union S union A`.
With demonstrated disjoint ownership, `|W| = 248 + |S| + |A|`; otherwise compute
the exact lane-set union and reject ownership conflicts. Record numeric |S|,
|A|, |W|, D and each instruction's occurrence maximum before execution. Repeated
writes are additionally constrained by finite event counts, not just |W|.

There is **no current numeric |S|/|A|/|W| or actual F witness**. The formula is a
closure obligation, not a filled footprint. A count with missing address members
cannot close F02. The4096 ceiling alone never grants4096 writable bytes.

Independent observation must supply a continuous sequenced physical write
stream, including same-value writes, with origin/PC/call-context/address/width/
old/new values, and separately capture all normal RAM endpoints. Reconcile
each event against exactly one word/lane/store rule and reconcile replay of
the observed writes with final snapshots. Event loss or unobserved hardware/
DMA/host writes leaves attribution UNVERIFIED; a known out-of-rule write FAILS.
Observer correctness requires independently authorized positive coverage for
each origin/width and loss path before relying on an empty subset. Neither a
guest log nor a matching endpoint hash establishes that calibration.

## R8. Closure

PASS only after an actual F word-map/store witness and observer qualification
are independently accepted and the future complete instance satisfies them.
Wrong region/value/origin/count or any forbidden unrelated-RAM mutation is FAIL.
Unassigned address, unknown store, broad mask, missing baseline/domain or silent/
uncalibrated write sensor is UNVERIFIED and blocks A1-E. No proof or observer
has been implemented/executed here; F02 remains partially resolved and blocked.
