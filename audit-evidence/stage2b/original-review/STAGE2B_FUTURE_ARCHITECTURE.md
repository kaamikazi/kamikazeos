# Bounded future-architecture compatibility audit

## Scope and honesty boundary

The AI-native/native-execution objectives in the owner's updated philosophy are
**research objectives, not implemented capabilities or proven outcomes**. This
review does not implement or import END Engine, Fibi, adaptive execution, AI
services, application transformation, GPU scheduling or Stage 2B fixtures.
KamikazeOS and The END Engine remain separate projects.

The question here is only whether Stage 2B introduces unnecessary coupling that
would obstruct later authorized work. It need not implement that work or relax
deterministic fault handling to accommodate it.

## 1. Native execution and ordinary isolation

**No mandatory-VM blocker was found in the Stage 2B design.** QEMU/QMP/GDB are
the present experimental testbench and independent observer, not a proposed
mandatory native-application virtualization layer. An emulator-validated x86
mechanism is not yet a physical-silicon validation claim.

The current kernel does not implement the future native-process architecture.
`kernel/src/ring3.c:12,45-54` explicitly holds execution approval at zero despite
foundation descriptors/pages. `paging.c:158-179` builds a static 16-MiB identity
map with one fixed user code page and four user stack pages. Keeping these exact
facts as frozen **Stage 2B baseline conditions** is reasonable; treating them as
permanent requirements for every future OS build would be an unnecessary blocker.

Future ordinary process isolation must still be kernel-enforced. The philosophy's
no-mandatory-VM preference does not authorize CPL3 access to supervisor memory,
device ports or privileged services. Stage 2B correctly authorizes none of those.

## 2. Real Ring 3 and user-space dynamic code

`emergency_record.c:10-29` requires kernel CS, known kernel text EIP and fixed
kernel stack classification for ordinary panic persistence. `emergency_frame.c`
can structurally decode the conditional CPL3 tail, but that is not a process
fault-disposition or native-user-application feature. `emergency.c:110-114` halts
unsafe contexts instead of terminating only one future user process.

These are real current limitations if reused unchanged as a future universal
application-fault policy. They are not evidence that Stage 2B must enable Ring 3
now, nor that legitimate user JIT code should be put into kernel text. Later
process-fault handling/ownership and authorized user executable-page policies
will need their own design, enforcement and verification.

Recommendation: label O/R-N/D/R-D as the present **kernel emergency profiles**.
Keep historical exact-build tests, then version/add appropriate later user-process
contracts rather than silently weakening the existing evidence. No rule in Stage
2B's design bans separately authorized user-space code generation/transformation.

## 3. Per-process profiling, reversible optimization and scheduling

No Stage 2B requirement forces a profiling service into the emergency path or
prevents later per-process profiling outside it. The prohibition on live code
patching during a controlled fault experiment protects causality; it is not a
general ban on future verified application instrumentation.

Exact ELF/hash/control-flow witnesses will naturally change for any future
authorized transformed artifact. Requiring a new witness is a verification gate,
not a reason to insert a mandatory VM. Preserve the original, transformed result,
correctness/performance evidence and rejected attempts separately. This audit
does not define or implement those deployment/rollback mechanisms.

Similarly, no GPU scheduling or hardware-aware optimizer is currently demonstrated
by Stage 2B. Future resource experiments must obey kernel permissions and remain
separate from bounded fault handling. The current no-SSE/soft-float critical-kernel
build is compatible with the DF path's hardware TS behavior; it does not imply a
permanent ban on authorized application SIMD/GPU use under a later context policy.

## 4. Multi-core: an actual source coupling, not a Stage 2B feature demand

The current design uses one global `emergency_entry_busy` and shared records/
entry state (`emergency.c:13-19`), one `kernel_tss`/privilege stack (`tss.c:7-9`),
one `df_task` (`double_fault.c:10`) and global GDT/IDT arrays. The fixed stack
classifier recognizes four named payloads (`emergency_frame.c:8-13`). These cannot
simply be relabelled SMP-safe or generalized to arbitrary per-thread kernel stacks.

Future multi-core execution needs separately reviewed per-CPU/task ownership,
architectural metadata and emergency-storage/output policy. The selector value
0x30 itself is not the blocker: separate per-CPU GDTs can use the same selector
number for different TSS objects. Sharing the present objects/state is the coupling.

Stage 2B's one-CPU requirement correctly bounds current evidence. It does not
forbid future SMP; no SMP implementation is required or authorized by this review.

## 5. AI/END independence and execution authority

The current emergency graph in `emergency_entry.S`, `emergency.c:90-137`,
`emergency_record.c:40-90` and `serial.c:43-79` is local assembly/C, fixed RAM and
bounded port polling. It contains no model RPC, planner, GPU task or END callback.
The boot message describing a host-side AI Recovery Core (`kernel.c:274-275`) is
not proof of an AI-native adaptive execution feature or authority delegation.

The design should preserve this boundary: a future user-space intelligence
service may propose experiments, but cannot make its agreement or availability
a prerequisite for exception delivery, safe halt, memory/resource permissions
or evidence validity. Kernel enforcement and independent correctness/performance
oracles remain separate from proposal generation. No automatic repair or self-
modification is authorized by the philosophy or this review.

Keeping failures immutable does not prohibit a future separately authorized
application rollback. It prohibits concealing/overwriting the failed experiment
or treating rollback as proof the original transformation was correct. Current
recovery/mutation rules stay in force until explicitly changed.

## Conclusion

No need to compromise deterministic fault handling, introduce mandatory app VMs,
or integrate an AI/END service was identified. Actual current static-layout,
kernel-fatal/user-disposition and shared-single-CPU limitations must not be
mistaken for completed native-process/SMP capability or permanent future policy.
They are separate later architecture work, not extra Stage 2B implementation
requirements. The main review's first-tranche oracle revisions remain necessary
independently of the long-term philosophy.
