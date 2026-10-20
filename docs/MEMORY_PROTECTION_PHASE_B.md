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
unready runtime/store state and active I/O cannot authorize persistence. These
predicates will be supplied by actual runtime checks during entry integration.

The emergency emitter has a 1024-character ceiling, fixed literal/hex formatting
and stops after sink failure. It performs no heap, logging, mapping or storage
operation. A sink must itself be bounded.

Production UART output now has a finite poll budget. `serial_putc_bounded` clamps
caller budgets to 100000 polls; bounded string/field output caps input at 4096
characters and stops on timeout. Existing normal strings remain within these
bounds. Emergency runtime output will use a smaller per-character budget and no
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

Task-gate and fatal-entry integration are subsequent Phase B increments. Real
fault validation remains separately authorized future work.
