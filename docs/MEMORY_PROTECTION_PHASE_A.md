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
guarantee. RAM-accounting observability and the final Phase A verification are
the next groundwork increment, not authorization for Phase B.
