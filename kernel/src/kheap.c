/* kheap.c - simple fixed-region bump allocator with bounds checks. */
#include "kheap.h"
#include "errors.h"
#include "memory.h"
#include "string.h"

#define KHEAP_ALIGN 16u

static uint32_t heap_start;
static uint32_t heap_end;
static uint32_t heap_cursor;
static uint32_t allocation_count;
static uint32_t failed_count;
static uint32_t high_water_mark;
static int available;

static uint32_t align_up(uint32_t value, uint32_t align)
{
    return (value + align - 1u) & ~(align - 1u);
}

void kheap_init(void)
{
    const struct memory_status *m = memory_status();
    heap_start = m->heap_start;
    heap_end = m->heap_end;
    heap_cursor = heap_start;
    allocation_count = 0;
    failed_count = 0;
    high_water_mark = 0;
    available = (heap_start != 0 && heap_end > heap_start);

    if (available) {
        error_log(SEV_INFO, "HEAP-000", "KernelHeap",
                  "Fixed bump heap initialized", "run heap for allocator stats");
    } else {
        error_log(SEV_WARNING, "HEAP-001", "KernelHeap",
                  "Kernel heap unavailable", "memory map left no safe heap range");
    }
}

void *kheap_alloc(uint32_t size)
{
    if (!available || size == 0) {
        failed_count++;
        error_log(SEV_WARNING, "HEAP-101", "KernelHeap",
                  "Allocation rejected", "size was zero or heap unavailable");
        return 0;
    }

    uint32_t aligned_cursor = align_up(heap_cursor, KHEAP_ALIGN);
    uint32_t aligned_size = align_up(size, KHEAP_ALIGN);
    if (aligned_size < size || aligned_cursor < heap_cursor ||
        aligned_cursor > heap_end || aligned_size > heap_end - aligned_cursor) {
        failed_count++;
        error_log(SEV_WARNING, "HEAP-102", "KernelHeap",
                  "Allocation rejected outside heap bounds", "request less memory");
        return 0;
    }

    void *ptr = (void *)aligned_cursor;
    heap_cursor = aligned_cursor + aligned_size;
    allocation_count++;
    uint32_t used = heap_cursor - heap_start;
    if (used > high_water_mark)
        high_water_mark = used;
    kmemset(ptr, 0, aligned_size);
    return ptr;
}

void kheap_reset_demo_only(void)
{
    heap_cursor = heap_start;
    allocation_count = 0;
    high_water_mark = 0;
}

void kheap_stats(struct kheap_stats *out)
{
    if (!out)
        return;
    out->available = available;
    out->heap_start = heap_start;
    out->heap_end = heap_end;
    out->total_bytes = available ? (heap_end - heap_start) : 0;
    out->used_bytes = available ? (heap_cursor - heap_start) : 0;
    out->remaining_bytes = available ? (heap_end - heap_cursor) : 0;
    out->allocation_count = allocation_count;
    out->failed_count = failed_count;
    out->high_water_mark = high_water_mark;
    out->free_supported = 0;
}

int kheap_available(void)
{
    return available;
}

uint32_t kheap_used(void)
{
    return available ? (heap_cursor - heap_start) : 0;
}

uint32_t kheap_remaining(void)
{
    return available ? (heap_end - heap_cursor) : 0;
}
