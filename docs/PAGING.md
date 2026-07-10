# KamikazeOS Paging

KamikazeOS v2.3 adds the first paging and virtual memory foundation.

## What Is Real

- 32-bit x86 page directory and page table structures.
- A low-memory identity map prepared in static kernel memory.
- Present/writable/supervisor page flags.
- Identity map validation.
- Paging status and page-map serial reports.
- Shell commands: `paging`, `page-map`, and guarded `pagefault-test confirm`.
- Page fault evidence path that records CR2, error code, and EIP when a page
  fault occurs.
- Analyzer and Recovery Center visibility.

## v2.3 Mode

```text
paging_mode=prepared_not_enabled
```

Paging is not enabled in v2.3. The kernel prepares and validates the paging
structures, but it does not load CR3 or set CR0.PG during normal boot.

## What Is Not Implemented

- Enabled paging in normal boot.
- Userspace virtual memory.
- Per-process address spaces.
- Demand paging.
- Copy-on-write.
- Memory protection between tasks.
- Page-frame allocation through the heap.

## Commands

```text
paging
page-map
pagefault-test confirm
```

`pagefault-test confirm` is guarded and refused while paging is disabled. It is
reserved for a future verified paging-enabled build.
