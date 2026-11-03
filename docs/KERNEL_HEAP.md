# KamikazeOS Kernel Heap

KamikazeOS v2.2 adds the first safe kernel heap allocator foundation.

## What Is Real

- A fixed heap range supplied by the memory manager.
- A simple bump allocator inside that bounded range.
- 16-byte allocation alignment.
- Rejection of zero-size and oversized allocations.
- Allocation count, failure count, used bytes, remaining bytes, and high-water
  stats.
- Shell commands: `heap`, `heap-test`, and `heap-alloc <bytes>`.
- Serial report section: `[KERNEL_HEAP]`.
- Analyzer and Recovery Center visibility.

## What Is Not Implemented

- `free()`.
- Heap growth.
- Virtual memory.
- Paging isolation.
- Userspace heap allocation.
- Per-process memory.
- malloc-compatible allocation semantics.

## Commands

```text
heap
heap-test
heap-alloc 64
```

`heap-test` performs small bounded allocations. `heap-alloc <bytes>` is a
manual test command; failed allocations are logged and do not return memory
outside the heap.
