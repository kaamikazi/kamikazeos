/*
 * kheap.h - v2.2 fixed kernel bump heap.
 *
 * This is a bounded kernel-only allocator. It is not malloc-compatible and
 * does not support free(), virtual memory, userspace heaps, or heap growth.
 */
#ifndef KMZ_KHEAP_H
#define KMZ_KHEAP_H

#include <stdint.h>

struct kheap_stats {
    int available;
    uint32_t heap_start;
    uint32_t heap_end;
    uint32_t total_bytes;
    uint32_t used_bytes;
    uint32_t remaining_bytes;
    uint32_t allocation_count;
    uint32_t failed_count;
    uint32_t high_water_mark;
    int free_supported;
};

void kheap_init(void);
void *kheap_alloc(uint32_t size);
void kheap_reset_demo_only(void);
void kheap_stats(struct kheap_stats *out);
int kheap_available(void);
uint32_t kheap_used(void);
uint32_t kheap_remaining(void);

#endif /* KMZ_KHEAP_H */
