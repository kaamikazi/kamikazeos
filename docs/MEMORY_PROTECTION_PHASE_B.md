# v2.6.2 Phase B — emergency diagnostics and double-fault architecture

Accepted Phase A baseline: `d564130ecf6f56f8ea3a9683f4ec78d8dd49c5e4`.
Branch: `feature/v2.6.2-memory-protection-phase-b`.

This work prepares emergency handling. It does not establish complete fault-path
safety acceptance. No intentional guest fault, real overflow/double fault or
CPL3 transition is authorized by this implementation run. NULL/guards stay
mapped, WP is not newly enabled, and PAE/NX/frame allocation remain out of scope.

## B1 primitives

`emergency_record.c` provides a RAM-only fixed 128-byte crash record, first-writer
preservation, a pure persistence-context classifier and bounded numeric output.
Its ABI contains no pointers or disk fields. Publication magic is written last.
Reentrant/DF contexts, malformed origins/stacks, low headroom, guard/table faults,
unready runtime/store state and active I/O cannot authorize persistence. Runtime
entry integration supplies these predicates from bounded checks of known state.

The emergency emitter has a 1024-character ceiling, fixed literal/hex formatting
and stops after sink failure. It performs no heap, logging, mapping or storage
operation. A sink must itself be bounded.

Production UART output now has a finite poll budget. `serial_putc_bounded` clamps
caller budgets to 100000 polls; bounded string/field output caps input at 4096
characters and stops on timeout. Existing normal strings remain within these
bounds. Emergency runtime output uses 4096 polls per transmitted character and no
normal formatter on unsafe paths. UART failure is best-effort output loss, not
a recursive error-log or persistence attempt.

Initial B1 verification: build succeeded; **14 passed, 0 failed/errors/skipped,
13.792 seconds** under `build/test-runs/v262-phase-b-b1/`. This includes 12
actual-C record/UART host cases and two ordinary Phase A QEMU cases with RAM
profiles. Host port outcomes are controlled; no CPU exception was triggered.

## B2 dedicated stack reservations

The linker and named policy now include separate 16-KiB fatal-diagnostic and DF
payloads, each with lower/upper 4-KiB reservations. The primary inventory grows
from 21 to 27 spans; guard reservations grow from six to ten. All ten remain
present/writable/supervisor in the legacy mapper. USER payload exposure remains
exactly one code page plus four stack pages. Page structures remain mutable.

The arrays occupy dedicated NOLOAD sections. ELF tests inspect every allocated
positive-size object: none may intersect any guard, and only the owning array
may occupy each emergency payload. The runtime stack accessor validates array
and linker bounds. The complete kernel reservation and heap candidate move after
these sections; existing shadow reservations include them symbolically.

The extended host/ELF and ordinary-boot selection completed **36 passed,
0 failed/errors/skipped, 15.058 seconds** in `build/test-runs/v262-phase-b-stacks/`.
This extension preserves the accepted Phase A policy and existing test IDs while
adding coverage for the new regions. It does not validate real overflow handling.

## B3 architecture, frame and I/O prerequisites

`x86_task.c` encodes the exact 104-byte 32-bit TSS, 8-byte TSS descriptor and
8-byte DPL0 task gate. Compile-time offsets cover CR3/EIP/ESP/I/O-map base.
Normal TSS selector `0x28` is retained; the added available DF TSS uses `0x30`.
Only the normal descriptor may validate as busy. The DF gate has zero offsets,
selector `0x30` and access `0x85`. The prepared task uses known kernel selectors,
EFLAGS `2`, a page-aligned CR3, isolated stack and an entry inside kernel text;
the validator compares every byte, including reserved fields. Descriptor/gate
APIs alone do not execute a task switch.

`emergency_frame.c` preserves the accepted 68-byte interrupt-frame prefix and
conditional 8-byte CPL3 tail inside a fixed 100-byte assembly/C entry state.
It validates stack classification, alignment and complete frame size before
reading CPU-frame words. No saved EIP, user ESP or PUSHA ESP is followed as a
pointer. PUSHA-time ESP keeps its historical meaning, distinct from reconstructed
same-CPL pre-delivery ESP. Frame/call/previous-TSS provenance is explicit in the
RAM record and persistence classifier. CPU-written TSS evidence is read through
volatile-qualified pointers.

ATA identify/read/write operations now carry an in-progress depth through their
existing retries. The guard releases on success/failure; rejected requests do no
I/O. Actual-C host port fixtures observe the guard during operations, including
absent disks and retry failures. Disk layouts and the PIO algorithms are retained.

The first integrated structural run exposed a real readiness failure: **53 pass,
1 failure** in `build/test-runs/v262-phase-b-architecture-first/`. Clang folded
`emergency_stacks_valid()` to false because separately declared C arrays and
external linker aliases were treated as distinct objects. Disassembly confirmed
the constant result. Loading the four linker addresses through volatile integer
slots preserves runtime address validation; the assertion was retained. The
corrected selection passed **54/54 in 18.994 seconds** under
`build/test-runs/v262-phase-b-architecture-corrected/`.

The final integration-focused selection also passed **54/54, zero failures,
errors or skips, in 20.803 seconds** under
`build/test-runs/v262-phase-b-integration/`. This includes descriptor/frame/ATA
host tests, linked-entry inspection, Phase A policy/accounting tests and ordinary
QEMU boot with read-only descriptor, register and full paging-table inspection.
Real fault validation remains separately authorized future work.

## B4 fatal entry and exception-only DF integration

The fatal ISR labels (0-31) and assembly `kpanic` claim a first-entry latch with
atomic BTS. Before changing ESP they perform no push/pop/call or interrupted-stack
dereference. The common entry saves registers to fixed RAM, reads CR2/CR0/CR3
before C/logging, selects kernel data segments, switches to the fatal stack and
calls C with 16-byte pre-call alignment. CPU-frame words or panic call arguments
are read only after complete bounds validation. IRQ and syscall paths retain
their existing entry/return ABI.

The first primary RAM record is published once. Numeric bounded output precedes
any normal formatter. An ordinary CPL0 exception/panic may reach existing
logging/persistence only with a valid boot/privilege-stack source, at least 1024
bytes of source headroom, expected CR3/kernel EIP, ready runtime and trusted
RecoveryStore/boot-order state, no active ATA operation and no selected unsafe
vector, guard or paging-structure fault. Panic strings are copied from classified
kernel spans into fixed 32/96/96-byte buffers before the normal formatter. The
exception formatter requires the exact validated fixed frame and uses early CR2.
Persistence rechecks readiness/store/I/O state before disk/CMOS work. Unsafe
contexts emit the minimal RAM/numeric evidence and halt.

Reentry cannot reuse/reset the primary record or fatal stack. Its assembly-only
path records separate minimal numeric evidence, prints a fixed message with a
64-character/4096-poll-per-character ceiling and halts. There is no C call,
allocation, mapping, store/CMOS, normal error log or task return on that path.
The main emitter caps logical output at 1024 characters; newline expansion can
transmit two UART bytes for a logical newline, each independently bounded.

`double_fault_init()` runs after paging activation. It requires loaded GDT/IDT/
normal TSS, valid esp0/emergency stacks/linker layout, validated legacy mappings,
the actual expected CR3 and trusted usable RAM backing the complete kernel.
It prepares the DF TSS before installing vector 8's task gate, and checks STR
still names the normal TSS. This is exception-only architecture, not scheduling.

DF entry runs on the task's new stack, captures early observed control registers,
bounds the hardware error-code read, aligns/reset its isolated ESP and calls only
the minimal DF recorder/emitter/halt path. Previous-task EIP/CS/EFLAGS/ESP is used
only with STR `0x30` and backlink `0x28`, and is labelled previous-TSS evidence,
not an original CPU interrupt frame. Its independent record never authorizes
persistence. It never returns/IRETs or clears task busy state.

Ordinary boot verification independently reads IDT vector 8, both TSS descriptor
types, the entire prepared DF TSS, actual control registers, all 1024 PDEs and all
4096 PTEs through the read-only QEMU monitor. It checks identity targets and leaf
rights throughout 16 MiB, exactly five USER leaves, all ten guards and NULL still
present/RW/supervisor, no WP/PAE activation, normal TR `0x28`, normal TSS busy and
DF TSS still available. Primary/DF/reentry record magic and entry counters remain
zero. These observations establish preparation and ordinary-boot compatibility.

## Limits and separately authorized fault-validation plan

No guest CPU exception, intentional panic, real overflow/DF, reentrant emergency
execution or Ring 3 transition was run. Host fixtures exercise production C with
controlled inputs/ports; disassembly and monitor checks establish structure, not
real fault-delivery correctness. There is no complete Phase B safety acceptance.

Fatal first stores assume the existing valid flat data selector and mapped fixed
RAM. The DF path depends on a usable GDT, TSS descriptors, known CR3 and stack;
arbitrary descriptor/page-table/RAM corruption or failed DF delivery can still
triple fault. CR2 is read before C/logging but after fixed entry stores; it is an
observed register, especially during DF, not guaranteed original-fault provenance.
The first latch is single-CPU architecture; no SMP fault coordination is provided.
UART budgets bound polls/output, not hardware wall-clock delivery guarantees.
The ATA guard covers device operations/retries, not every higher-level mutable
kernel transaction; eligibility checks do not prove arbitrary corrupted state
safe. Guards and NULL remain mapped and do not yet catch actual overflow/access.

After independent source/ABI/persistence review and separate explicit approval:

1. Use preserved exact binaries, disposable copied images, serial capture and a
   host timeout/reset harness. Record source/ELF/image hashes and expected outcomes.
2. Validate one ordinary same-CPL exception/panic at a time: CPU frame/error shape,
   early CR2, source vs diagnostic ESP, primary-record publication, bounded output,
   permitted persistence and next-boot/analyzer interpretation. The four deferred
   historical cases must be independently rerun rather than inferred from old runs.
3. Validate explicit unsafe contexts individually (bad/low-headroom stack evidence,
   active ATA operation, paging/guard classification), proving no new disk/CMOS
   writes and no entry into the normal formatter/logging path.
4. In a separately designed disposable fault-injection build, verify reentry during
   capture/output, immutable first evidence, separate secondary evidence, bounded
   UART failure and terminal halt. Do not assume host mock recursion proves this.
5. Finally authorize controlled DF delivery and then actual overflow experiments:
   prove hardware task switch, CR3/selector/stack state, backlink provenance,
   isolated DF evidence, no persistence/task return and bounded host-visible halt.
   Keep guard/WP activation a later separately reviewed phase.

Each real-fault result needs its own evidence and review. Structural preparation
does not authorize activating later memory protections or marking these paths safe.
