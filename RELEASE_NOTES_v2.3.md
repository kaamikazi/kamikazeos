# KamikazeOS v2.3.0 "Paging Gate"

v2.3 is the first KamikazeOS release to enable 32-bit x86 paging. A strict
boot-time gate validates the kernel-owned GDT, early exception IDT, paging
structures, supervisor flags, active CR3, and every required runtime range
before setting CR0.PG.

The release adds named identity-map evidence, `[PAGING]`, `[PAGE_MAP]`, and
`[PAGE_FAULT]` reports, guarded fault testing, analyzer interpretation, and a
read-only Recovery Center page-fault panel. Normal QEMU regression testing
covers interrupts, PIT ticks, IRQ keyboard input, tasks, heap, ATA-backed
recovery state, TinyFS, built-in apps, and the package lifecycle under paging.

This remains an experimental OS research prototype, not a production or
general-purpose operating system. The mapping is supervisor identity-only.
There is no Ring 3 userspace, process isolation, ELF loading, syscall layer,
demand paging, networking, automatic repair, binary patching, downloads,
arbitrary code execution, or kernel self-modification.

Known pre-v2.3 limitations remain documented: PS/2 validation is QEMU-focused,
package interruption recovery has not been destructively tested, TinyFS
metadata is not fully transactional, and some legacy RecoveryStore reset
writes do not propagate every storage failure.
