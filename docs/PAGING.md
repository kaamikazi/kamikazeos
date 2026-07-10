# KamikazeOS Paging

KamikazeOS v2.3 "Paging Gate" adds the first enabled paging foundation.

## Enabled Mode

The kernel prepares four 32-bit x86 page tables and identity maps physical and
virtual addresses from 0 through 16 MiB. Entries are present, writable, and
supervisor-only. The kernel loads CR3 and sets CR0.PG only after validation.

The gate verifies:

- the kernel-owned GDT and early exception IDT are loaded
- page directory and tables are 4 KiB aligned
- mapped PDE/PTE addresses and flags are exact
- unused page-directory entries remain absent
- low memory, VGA text memory, kernel image/static buffers, bootstrap stack,
  fixed heap, page directory, and page tables are identity mapped

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

## Boundaries

The enabled identity map does not provide userspace, process isolation,
per-process address spaces, demand paging, copy-on-write, swapping, or memory
protection between cooperative tasks.
