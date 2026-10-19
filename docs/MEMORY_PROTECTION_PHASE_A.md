# v2.6.2 Memory Protection — Phase A groundwork

Baseline: accepted v2.6.1 commit
`fb5b58f69249dd22ea7e0e0f42a055c6584ac3f5`. Development branch:
`feature/v2.6.2-memory-protection-phase-a`.

## Boundaries of this phase

This is descriptive groundwork, **not protection activation**. The existing
non-PAE i686 mapper still presents all 4096 pages in the bounded 16 MiB aperture.
Only the existing one-page user-code payload and four-page user-stack payload
have USER permissions. NULL, kernel text/rodata, page structures and reserved
guards retain their legacy present/writable supervisor policy in this phase.

CR0.WP is not enabled by this work. No PAE/NX, double-fault task gate, general
frame allocator, new fault entry point or CPL3 transition is introduced. Read-only
page intent does not imply non-executable memory on this paging architecture.

## Linker-derived classification

`kernel/linker.ld` explicitly collects `.text.*`, `.rodata.*` (including merged
strings), `.data.*`, `.bss.*` and user subsection families. It publishes aligned,
end-exclusive spans for text, rodata, data, bss, directory, tables, bootstrap and
privilege stack payloads, user payloads, six guard reservations and a heap
candidate. Summary image bounds are not overlapping primary regions.

Page structures occupy dedicated NOLOAD sections. The three existing 16-KiB stack
payloads each have a lower and upper 4-KiB reservation. These reservations are
**still mapped**, not active guards. `_kernel_end` includes the final user guard;
the 64-KiB heap candidate follows it and still requires the existing runtime RAM
selection. No diagnostic/DF stack is allocated in Phase A.

Link-time checks enforce stack/table/user sizes, assembly-bound agreement,
ordered boundaries and aperture bounds. Pure C validation additionally checks
every primary region for alignment, nonempty/nonwrapped intervals, exact fixed
sizes, order and adjacency. ELF tests reject unexpected allocatable orphan
sections and check the actual linked objects and complete subsection coverage.

## Central permission model

`memory_policy.c` is production pure C shared with deterministic host tests.
`memory_layout.c` binds its inputs to linker symbols and the selected heap.

| Region | Active legacy flags | Future plan flags with validated usable RAM |
|---|---|---|
| Text/rodata/directory/tables | present writable supervisor | present read-only supervisor |
| Data/bss/kernel stacks | present writable supervisor | present writable supervisor |
| NULL/low reserved/stack guards | present writable supervisor | absent |
| VGA | present writable supervisor | explicit supervisor device exception |
| User code, one page | present read-only user | same |
| User stack, four pages | present writable user | same |
| Heap candidate | legacy mapping, runtime heap selection unchanged | writable only with available heap and usable RAM |

Only `MP_LEGACY_ACTIVE` is used by the paging validator; the future plan is not
used to program tables. Table construction and `paging_enable.S` retain the
accepted paging behavior. The pure policy fails closed on malformed layouts and
out-of-aperture queries. It contains only P/RW/USER flags, no fictional NX bit.

## Initial verified layout/policy increment

- Build: `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1`.
- Focused verification: `python -B tests/run_regression.py --no-build --run-dir
  build/test-runs/v262-phase-a-layout-first -k M1PagePolicyC -k M1LinkedLayout -k M1LayoutBoot`.
- Result: **13 passed, 0 failures/errors/skips, 5.874 seconds**.
- Includes all 4096 legacy leaf-policy decisions and exactly five USER pages,
  invalid spans, intended section flags, inactive guards, read-only ELF/static
  layout checks and ordinary QEMU boot with heap/task commands and Ring 3 held.

These checks do not establish active write protection, active guards or an NX
guarantee. The subsequent accounting increment below remains Phase A, not
authorization for Phase B.

## RAM-accounting observability

The legacy bitmap, heap selection and public legacy counts are unchanged.
`memory_accounting.c` supplies a separate pure, bounded model. The memory parser
retains up to 64 full 64-bit map spans alongside its existing clipped tracker.
The new model computes a reserved-wins usable-byte union without counting
overlapping usable entries twice. Detection is separate from the highest
physical address and from whole-page eligibility.

| Quantity | Meaning |
|---|---|
| Described usable bytes | normalized usable union described by the input; may be a fallback estimate |
| Detected usable bytes | same union only for validated/trusted discovery; otherwise zero |
| Tracked usable pages | whole usable pages within the legacy tracker (maximum 128 MiB) |
| Mapped present pages | active legacy 16 MiB aperture: 4096 leaves |
| Mapped usable/non-RAM pages | the active aperture partitioned by whole-page RAM classification |
| Reserved usable pages | tracked usable pages reserved for low memory, complete kernel/guards, selected heap or known boot metadata |
| Unmapped usable pages | tracked usable pages outside the active aperture (or all when paging is held) |
| Eligible pages | trusted, tracked, mapped supervisor-writable usable pages minus runtime/boot reservations; descriptive only |

`[MEMORY_POLICY]`, `[MEMORY_REGIONS]` and `[RAM_ACCOUNTING]` are additive serial
sections. They explicitly report `protected_plan_installed=no`,
`guard_pages_active=0`, `page_structures_mutable=yes`, `nx_enforced=no`,
`frame_allocator=no`, and `eligibility_enforced=no`. Existing report sections and
legacy counts retain their meaning. The release banner remains at the baseline
version while the new section identifies `v2.6.2_phase_a` groundwork.

Untrusted fallback maps never claim detected RAM or eligible frames. Capacity
exhaustion holds the new accounting rather than presenting partial data as
complete; it does not widen or change the legacy parser/allocator behavior.
Known Multiboot prefix/mmap pages are conservatively reserved for accounting.
Unclassified boot modules or failed legacy memory validation hold candidate
eligibility. Optional boot objects are not imported or loaded by this phase;
complete boot-object lifetime and allocation enforcement belong to later approval.

Metrics are cached after the layout exists, keyed by whether the legacy validated
mapping is active. They never write page tables, alter the allocation bitmap,
allocate frames, enable WP or change persistent state.

## Accounting and profile controls

`v262-phase-a-metadata-profiles` completed **23 cases, 0 failures/errors/skips,
13.314 seconds**: 18 actual-C host cases, three ELF/static cases and two ordinary
QEMU cases (including 4/16/128 MiB profile boots). Host vectors include the
4/8/16/32/64/128/256 MiB matrix, >4-GiB discovery, partial/adjacent pages,
reserved overlap, extra boot reservations, unavailable heap, bad limits and
untrusted discovery. No guest fault trigger is executed.

All 148 baseline regression IDs remain discoverable. Phase A's broad compatibility
run explicitly omits only the three `L3Ring0Faults` cases and
`test_intentional_panic_test_not_penalized_next_boot`. Existing storage-integrity
cases use disposable media/controlled I/O outcomes; no CPU exception/panic test
is intentionally invoked. `--exclude` writes the exact omitted IDs to a separate
receipt; exclusions are neither passed cases nor unittest skips.

## Remaining limits

The protected plan is not active, and its runtime guard/fault substrate is absent.
NULL remains mapped, kernel code/data remain writable, and all present data is
potentially executable in non-PAE mode. Old heap/fallback allocation rules are
not tightened by shadow eligibility. No general frame allocator is introduced.
GDT/IDT and double-fault entry are unchanged. No diagnostic/DF stacks are added.
These are deliberate phase boundaries, not claims of completed memory protection.

Linker relocation changes absolute addresses and reserves six additional pages;
consumers must use symbols rather than old addresses. Input capacities and
accounting trust holds are explicit. Runtime fault handling, fallback enforcement,
safe allocation policy and protection activation remain pending separate approval.
Accepted v2.6.1 storage formats/authority rules and historical evidence are intact.

## Final verified linker layout

Intervals are end-exclusive and describe the final Phase A test ELF, not ABI
addresses for future builds. All listed boundaries are page-aligned. Low memory
is separately classified as NULL `[0,0x1000)`, legacy low memory up to VGA,
VGA `[0xB8000,0xB9000)`, and legacy low memory through `0x100000`.

| Linker region | Start | End |
|---|---|---|
| Text | `00100000` | `00126000` |
| Rodata, including string subsections | `00126000` | `00137000` |
| Data | `00137000` | `00138000` |
| Bss | `00138000` | `00143000` |
| Page directory | `00143000` | `00144000` |
| Page tables | `00144000` | `00148000` |
| Bootstrap lower guard reservation | `00148000` | `00149000` |
| Bootstrap payload | `00149000` | `0014D000` |
| Bootstrap upper guard reservation | `0014D000` | `0014E000` |
| Privilege lower guard reservation | `0014E000` | `0014F000` |
| Privilege payload | `0014F000` | `00153000` |
| Privilege upper guard reservation | `00153000` | `00154000` |
| User code, one page | `00154000` | `00155000` |
| User lower guard reservation | `00155000` | `00156000` |
| User payload, four pages | `00156000` | `0015A000` |
| User upper guard reservation | `0015A000` | `0015B000` |
| Heap candidate, 64 KiB | `0015B000` | `0016B000` |

`_kernel_end` is `0015B000`; heap backing still needs runtime RAM validation.
The complete primary region list and raw symbol values are retained in
`build/test-runs/v262-phase-a-compatibility/layout.json`.

## Final Phase A verification and preservation

The accounting/observability increment was verified after layout-policy commit
`b55e180f95ef7d87119618106d9294b7d8d68749`. Its completed selected run is:

```text
build/test-runs/v262-phase-a-compatibility/
Ran 167 tests in 975.953s
OK
runner exit code: 0
```

Counts: **167 passed, 0 failed, 0 errors, 0 skipped, 0 incomplete selected cases**.
This comprises **144 original cases plus 23 new Phase A cases** (18 actual-C
host cases, three ELF/static cases and two ordinary QEMU cases). All 148 original
IDs remain registered; four guest CPU-fault/panic cases are explicitly excluded,
not counted as passes or skips. This is not a claim that all 171 discovered cases
were executed. Their exact IDs are in `excluded-tests.txt`; selection is recorded
in `selected-tests.txt`, with actual exit/counts in `result.json`.

Reproduction after building, using the preserved compatibility-baseline ELF:

```powershell
python -B tests/run_regression.py --no-build --baseline-elf "C:\Users\imran\AppData\Local\Temp\opencode\kmz-release-accept-fb5b58f\verified\baseline\build\kamikaze.elf" --run-dir "build/test-runs/v262-phase-a-compatibility-new" --exclude L3Ring0Faults --exclude test_intentional_panic_test_not_penalized_next_boot
```

The run exercises existing package/reset authority, RecoveryStore, TinyFS,
snapshot rotation, host consumers and normal boot/subsystems on disposable
images. Only the four deferred CPU/panic cases are omitted. The new ordinary
boot checks exercise heap/task commands, validated legacy paging, mutable page
structures, inactive guards, exact user exposure and RAM profiles without
intentional guest faults or CPL3 execution.

The original directory is preserved; choose a fresh directory for reproduction.
Test time is 975.953 seconds; wrapper wall time is 976.164 seconds. The full build
and actual-C compiles completed with the existing warning flags. Additional
`-fstack-usage` inspection measured static frames of 2460 bytes (`memory_init`),
2112 bytes (`install_fallback`) and 1084 bytes (`usable_union`); those are
per-function values, not a proof of a guarded whole-call-chain budget.

| Artifact | SHA-256 |
|---|---|
| Tested Phase A `build/kamikaze.elf` | `8fe909ac0c855690b4d1cfef8be3bbe9c9603c131514908893e1c99b22e7837d` |
| Completed `summary.txt` | `81d7da98fcfb6391bae72bed36166a9322446cfbec5b1f920bb2aa77ed528e99` |
| Completed `result.json` | `11dbb1b1d766fcfbb13300205c926d6bc37b2d29b38b402187c8b7b433d3bb20` |
| Original `build/recovery.img` | `4daf871c5c2982a2c6401a8a3336959f31bdd0143ae00f238f4cbdc7f453f19e` |

The accepted baseline branch still points at
`fb5b58f69249dd22ea7e0e0f42a055c6584ac3f5`. Its source archive, original ELF,
recovery-image copy and initial SHA/test-ID inventory are additionally preserved
in `build/preservation/v261-accepted-fb5b58f-phase-a/`. The preserved original ELF
has SHA-256 `c1c9458e3cc6e7491decef8aef624e553252c55efed2d35d5b766a358aad1c78`.

The compatibility preflight verifies byte-identical accepted source for paging
activation assembly, GDT/IDT, Ring 3 entry/gate, exception stubs/panic and storage
writers; the legacy table-construction body is also unchanged. No WP activation,
new active mappings, double-fault task gate, PAE/NX, general frame allocator or
intentional guest fault entry point is included. All historical audit hashes and
the original recovery image matched after execution; code and fixtures did not
change during the run. Subsequent edits only record results in Markdown.

Phase A is complete for review. Phase B and all protection/fault activation remain
pending separate approval; this is not independent release acceptance.
