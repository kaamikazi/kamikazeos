# KamikazeOS Memory Map

KamikazeOS v2.1 adds the first kernel memory management foundation.

## What Is Real

- Multiboot v1 memory-map parsing when the bootloader provides `mmap_addr`.
- Safe fallback memory layout when the memory map is unavailable.
- Fixed 4 KiB physical page accounting.
- Low-memory reservation below 1 MiB.
- Kernel image reservation using linker-provided `_kernel_start` and
  `_kernel_end`.
- Shell commands: `mem`, `memmap`, and `pages`.
- Serial report sections: `[MEMORY]`, `[MEMORY_MAP]`, and `[PAGES]`.
- Analyzer and Recovery Center visibility.

## What Is Not Implemented

- Virtual memory.
- Paging isolation.
- Ring 3 userspace memory.
- `malloc/free` heap allocation.
- Per-process address spaces.
- Demand paging, swapping, or copy-on-write.

## Commands

```text
mem
memmap
pages
```

`mem` shows totals, kernel bounds, and the heap placeholder. `memmap` shows
usable and reserved physical regions. `pages` shows the fixed 4 KiB page
tracker counts.
