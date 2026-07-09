/*
 * memory.h - v2.1 physical memory discovery and page accounting.
 *
 * This is not virtual memory. It records the Multiboot memory map when
 * available, keeps a fixed-size 4 KiB physical page tracker, and publishes a
 * bounded kernel heap range for the bump allocator.
 */
#ifndef KMZ_MEMORY_H
#define KMZ_MEMORY_H

#include <stdint.h>

#define MEMORY_MAX_REGIONS 16
#define MEMORY_PAGE_SIZE   4096u

enum memory_region_type {
    MEM_REGION_USABLE   = 1,
    MEM_REGION_RESERVED = 2,
};

struct memory_region {
    uint32_t base;
    uint32_t length;
    uint32_t type;
};

struct memory_status {
    int map_available;
    int fallback_used;
    uint32_t region_count;
    uint32_t total_kib;
    uint32_t usable_kib;
    uint32_t reserved_kib;
    uint32_t kernel_start;
    uint32_t kernel_end;
    uint32_t heap_start;
    uint32_t heap_end;
    const char *allocation_mode;
};

struct page_status {
    uint32_t page_size;
    uint32_t total_pages;
    uint32_t free_pages;
    uint32_t used_pages;
    uint32_t reserved_low_pages;
    uint32_t reserved_kernel_pages;
    uint32_t reserved_heap_pages;
    uint32_t tracked_limit_kib;
};

void memory_init(uint32_t mb_magic, uint32_t mb_info);
const struct memory_status *memory_status(void);
void memory_pages(struct page_status *out);
uint32_t memory_region_count(void);
int memory_region_get(uint32_t index, struct memory_region *out);
const char *memory_region_type_name(uint32_t type);
int memory_map_available(void);

#endif /* KMZ_MEMORY_H */
