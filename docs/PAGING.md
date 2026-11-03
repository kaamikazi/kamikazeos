# KamikazeOS Paging

KamikazeOS v2.3 "Paging Gate" adds the first enabled paging foundation.

## Enabled Mode

The kernel prepares four 32-bit x86 page tables and identity maps physical and
virtual addresses from 0 through 16 MiB. The map is supervisor-writable by
default. v2.6 changes only one linked code PTE to user/read-only and four fixed
stack PTEs to user/writable. The kernel loads CR3 and sets CR0.PG only after
validation.

The gate verifies:

- the kernel-owned GDT and early exception IDT are loaded
- page directory and tables are 4 KiB aligned
- mapped PDE/PTE addresses and flags are exact
- unused page-directory entries remain absent
- low memory, VGA text memory, kernel image/static buffers, bootstrap stack,
  fixed heap, bootstrap stack, privilege-transition stack, page directory, and
  page tables are identity mapped
- the user code and stack permissions exactly match their linked bounds
- every kernel and paging-structure range remains supervisor-only
- the total USER PTE count equals the five expected pages

If validation fails, boot continues in `prepared_not_enabled` mode and logs
the validation reason. The gate is not bypassable through the shell.

## Page Faults

Vector 14 reads CR2, records the fault address, CPU error code, and EIP, emits
`[PAGE_FAULT]` serial evidence, persists panic evidence, and halts. KamikazeOS
does not attempt page-fault recovery.

```text
paging
page-map
pagefault-test confirm
```

`pagefault-test confirm` deliberately reads an unmapped high address and must
only be run last in a disposable QEMU session using a copied recovery image.
`user-fault-test confirm` is prepared to enter CPL3 and read supervisor-only
kernel memory; its error code, saved CPL3 frame, CR2 address, user ESP, and
user SS would be reported before halt. The post-hardening baseline keeps this
independent destructive-test gate disabled; it was not part of the approved
round-trip test.

## Boundaries

Five present pages are intentionally user-accessible for the fixed test. Every
other present page remains supervisor-only. The enabled identity map does not
provide general userspace, process isolation, per-process address spaces,
demand paging, copy-on-write, swapping, guard pages, or memory protection
between cooperative tasks.
