# Future A1 Build Contract: A1-UD-1020

**NOT IMPLEMENTED / NOT BUILT. Every future numeric binding is UNASSIGNED.**
This contract specifies what an independently reviewable future build F must
contain. P is the existing production reference; its addresses cannot substitute
for a binding to F. This document is not a source patch or a fixture.

## B1. Immutable Case And Build Identity

F must derive from production dbf23ce74a2a7103ae6fce04fba51969b7807178 in a fresh,
separately authorized isolated preparation directory. Production and historical
artifacts are never build outputs. Freeze exact source diff/file hashes, compiler/
linker versions and binary hashes, full flags, linker map, ELF and allocatable
section bytes, source-to-disassembly witness, observer/model/CPU/firmware hashes
and full launch topology. One CPU; legacy mappings; SS32 flat0x10, CS0x08,
TR0x28, CR4=0, unchanged root/policy; runtime and DF readiness true.

No hidden sanitizer/profiler/trampoline/red-zone/stack-protector instrumentation,
code patch, protection activation, Ring3 grant, policy relaxation or alternate
fault instruction is permitted. Any compiler-generated store/call is part of
the reviewed graph. Existing machine code may move after linking; derive all
sites again. Build permission is a later explicit owner decision.

## B2. Fixture-Owned State: Exact Derivation Rule

Reserve exactly two aligned32-bit objects with unique proposed symbols
`stage2b_a1_arm` and `stage2b_a1_ack`. Their future numeric addresses are not known.
Their defining input section is proposed `.bss.stage2b_a1_state`, length8/
alignment4, collected by the existing `.bss.*` rule into F's `.bss` output.
These are newly allocated objects with their own exact leaf ownership, not a
reservation of the whole BSS output. This avoids introducing a new gap into the
current contiguous linker-layout contract. Section envelopes describe placement;
the eight fixture bytes have only the fixture leaf owner. Any linker padding is
immutable and explicitly owned as padding, not fixture scratch. Do not alias an
existing record, stack or arbitrary existing BSS gap.

The build receipt must state both VA/PA, section/load owner, exact8-byte extent,
zero initialization, disjointness and page mapping. Require
ack=arm+4, total size8, no aliases, no overlap with production globals, paging
structures, GDT/IDT/TSS, kernel text/rodata, heap, stacks, guards or reserved
device memory. Containment in the BSS section envelope is permitted; aliasing
any other object's byte owner is not.
The one eight-byte input section is the entire proposed new RAM allocation for A1;
additional counters/buffers require a new preparation scope. No section exists now.

Normal arm writes0->1 only on an explicit future arm action, then consumes
arm1->0/ack0->1 on the healthy stack before the strict fault epoch. Neither word
may change after E-fault. The arm is default-off; unarmed flow cannot pivot/fault.
Consume once per fresh instance. No automatic retry or reused evidence instance.

## B3. Semantic Labels And Exact Instruction Witness

Proposed future labels identify these boundaries; actual symbols/PCs/bytes are
required, not asserted present:

| Label | Required state |
|---|---|
| `a1_live_before_pivot` | After arm consumption and CLI, on healthy bootstrap stack; ESP_live>=B+4096. No remaining source-stack store before the pivot. |
| `a1_pivot` | One reviewed load/move of ESP to S=B+1032. Its addressing mode must not read a mutable pointer or spill after the baseline. |
| `a1_ud2` | Immediately next instruction after the completed pivot; bytes0f0b. No CALL/PUSH/spill/index read/ack store/debug trap between pivot and UD2. E-fault is here, IF0. |

The original live continuation is abandoned by design; no stack restore, IRET
or normal return is allowed. `[ESP_live,T)` must be entirely in the exact F
bootstrap payload and disjoint from `[B,B+1032)`. Snapshot the whole payload,
not only a frame-pointer guess. Static call/unwind/store inspection must establish
that no live automatic storage lies below the selected healthy SP at this point.

Read-only nonpatching observation may pause at labels only after separate
observer review. It must not insert INT3, change ESP/EIP/flags, call guest C or
manufacture a CPU exception. A preinstruction debugger stop is a capture boundary,
not evidence the later #UD occurred. Resume must be tied to an actual vector6
event at the same `a1_ud2`, guest-origin i=0, without vector8/reset/reentry.

## B4. Exact Future Binding Manifest (Required, Absent)

The future `A1_BUILD_BINDING.json` must contain the following closed fields and
numeric extents. No null, wildcard, P-reference-only value or unresolved symbol
is acceptable at A1-B:

- case/version and source/ELF/tool/observer/model/launch digests;
- all entry/label/caller/terminal PCs, instruction bytes and corresponding
  reachable CFG; decoded direct/indirect call targets;
- bootstrap B/T, S=B+1032, frame[B+1020,B+1032), fatal Fbottom/Ftop,
  primary/entry/DF/aux/latch/authorization/runtime globals and exact sizes;
- arm/ack section, VA/PA and mapping; all padding and other owner extents;
- root/page table/GDT/IDT/normal-TSS/DF-TSS bounds and actual descriptors;
- full memory-topology and mapping inventory, with translation evidence for
  every relevant VA and physical owner;
- exact A1 O unsafe-halt CLI/HLT/backedge PCs and distinct rejected dispatcher
  fallback; no permissive set of every HLT;
- B-RAM per-word map, per-PC store rules, stack/store union, numeric maximum
  distinct-byte footprint and occurrence bound;
- W-ENTRY registry digest/capacity receipt and CMOS-T1 qualification identifiers.

P reference facts: bootstrap[0x14e000,0x152000), fatal[0x15a000,0x15e000),
source frame[0x14e3fc,0x14e408); common pivot0x1000ea, first CALL0x1000f2,
unsafe post-HLT0x102ec2, fallback post-HLT0x102eb2. F must bind its own addresses.
The existing trigger's prologue at0x107c30 before UD2 at0x107c33 cannot be called
after the new pivot as a substitute.

## B5. Gates And Verdict

PASS at build review means all actual F artifacts exist, match, are independently
decoded and every field/graph/ownership condition holds. It is only build-review
PASS, not A1 behavior PASS. A concrete wrong opcode/geometry/overlap/extra
instrumentation is FAIL. Absent build, unresolved target/store, uncertain map,
unqualified observer or missing calibration is UNVERIFIED and blocks A1-E.

Current B5 status: **UNVERIFIED / U-BUILD OPEN**. No future fixture or numeric
store-footprint manifest can be produced under this design-only task.
