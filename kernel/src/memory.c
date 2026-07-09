/* memory.c - Multiboot memory map parsing and fixed 4 KiB page tracking. */
#include "memory.h"
#include "errors.h"
#include "string.h"

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002u
#define MB_INFO_FLAG_MEM      (1u << 0)
#define MB_INFO_FLAG_MMAP     (1u << 6)

#define MAX_TRACKED_PAGES     32768u      /* 128 MiB, enough for demo QEMU */
#define PAGE_BITMAP_BYTES     (MAX_TRACKED_PAGES / 8u)
#define LOW_MEMORY_LIMIT      0x00100000u
#define FALLBACK_MEMORY_BYTES 0x01000000u /* conservative 16 MiB */
#define KERNEL_HEAP_BYTES     0x00010000u /* 64 KiB fixed bump heap */

struct multiboot_info_v1 {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
} __attribute__((packed));

struct multiboot_mmap_entry {
    uint32_t size;
    uint32_t addr_low;
    uint32_t addr_high;
    uint32_t len_low;
    uint32_t len_high;
    uint32_t type;
} __attribute__((packed));

extern char _kernel_start;
extern char _kernel_end;

static struct memory_status status;
static struct memory_region regions[MEMORY_MAX_REGIONS];
static uint8_t page_bitmap[PAGE_BITMAP_BYTES]; /* bit=1 means used/reserved */
static uint32_t page_limit;
static uint32_t reserved_low_pages;
static uint32_t reserved_kernel_pages;
static uint32_t reserved_heap_pages;

static uint32_t align_down(uint32_t value, uint32_t align)
{
    return value & ~(align - 1u);
}

static uint32_t align_up(uint32_t value, uint32_t align)
{
    return (value + align - 1u) & ~(align - 1u);
}

static uint32_t min_u32(uint32_t a, uint32_t b)
{
    return a < b ? a : b;
}

static void bitmap_set(uint32_t page)
{
    if (page < MAX_TRACKED_PAGES)
        page_bitmap[page / 8u] |= (uint8_t)(1u << (page % 8u));
}

static void bitmap_clear(uint32_t page)
{
    if (page < MAX_TRACKED_PAGES)
        page_bitmap[page / 8u] &= (uint8_t)~(1u << (page % 8u));
}

static int bitmap_used(uint32_t page)
{
    if (page >= MAX_TRACKED_PAGES)
        return 1;
    return (page_bitmap[page / 8u] & (uint8_t)(1u << (page % 8u))) != 0;
}

static void add_region(uint32_t base, uint32_t length, uint32_t type)
{
    if (length == 0)
        return;

    if (status.region_count < MEMORY_MAX_REGIONS) {
        regions[status.region_count].base = base;
        regions[status.region_count].length = length;
        regions[status.region_count].type = type;
        status.region_count++;
    }

    status.total_kib += length / 1024u;
    if (type == MEM_REGION_USABLE)
        status.usable_kib += length / 1024u;
    else
        status.reserved_kib += length / 1024u;
}

static void mark_range_free(uint32_t base, uint32_t length)
{
    uint32_t start = align_up(base, MEMORY_PAGE_SIZE) / MEMORY_PAGE_SIZE;
    uint32_t end = align_down(base + length, MEMORY_PAGE_SIZE) / MEMORY_PAGE_SIZE;
    if (end > MAX_TRACKED_PAGES)
        end = MAX_TRACKED_PAGES;
    for (uint32_t p = start; p < end; p++)
        bitmap_clear(p);
}

static int range_is_usable(uint32_t base, uint32_t length)
{
    uint32_t end = base + length;
    if (end < base)
        return 0;
    for (uint32_t i = 0; i < status.region_count; i++) {
        if (regions[i].type != MEM_REGION_USABLE)
            continue;
        uint32_t rbase = regions[i].base;
        uint32_t rend = regions[i].base + regions[i].length;
        if (rend < rbase)
            continue;
        if (base >= rbase && end <= rend)
            return 1;
    }
    return 0;
}

static uint32_t mark_range_used(uint32_t base, uint32_t length)
{
    uint32_t changed = 0;
    uint32_t start = align_down(base, MEMORY_PAGE_SIZE) / MEMORY_PAGE_SIZE;
    uint32_t end = align_up(base + length, MEMORY_PAGE_SIZE) / MEMORY_PAGE_SIZE;
    if (end > MAX_TRACKED_PAGES)
        end = MAX_TRACKED_PAGES;
    for (uint32_t p = start; p < end; p++) {
        if (!bitmap_used(p))
            changed++;
        bitmap_set(p);
    }
    return changed;
}

static void init_bitmap(void)
{
    kmemset(page_bitmap, 0xff, sizeof(page_bitmap));

    for (uint32_t i = 0; i < status.region_count; i++) {
        if (regions[i].type == MEM_REGION_USABLE)
            mark_range_free(regions[i].base, regions[i].length);
    }

    reserved_low_pages = mark_range_used(0, LOW_MEMORY_LIMIT);
    if ((uint32_t)&_kernel_end > (uint32_t)&_kernel_start) {
        reserved_kernel_pages = mark_range_used((uint32_t)&_kernel_start,
                                                (uint32_t)&_kernel_end - (uint32_t)&_kernel_start);
    }
    if (status.heap_end > status.heap_start)
        reserved_heap_pages = mark_range_used(status.heap_start,
                                             status.heap_end - status.heap_start);
}

static void fallback_map(void)
{
    status.map_available = 0;
    status.fallback_used = 1;
    add_region(0, LOW_MEMORY_LIMIT, MEM_REGION_RESERVED);
    add_region(LOW_MEMORY_LIMIT, FALLBACK_MEMORY_BYTES - LOW_MEMORY_LIMIT,
               MEM_REGION_USABLE);
}

void memory_init(uint32_t mb_magic, uint32_t mb_info)
{
    kmemset(&status, 0, sizeof(status));
    kmemset(regions, 0, sizeof(regions));
    status.kernel_start = (uint32_t)&_kernel_start;
    status.kernel_end = (uint32_t)&_kernel_end;
    status.heap_start = align_up(status.kernel_end, MEMORY_PAGE_SIZE);
    status.heap_end = 0;
    status.allocation_mode = "physical_page_tracker_plus_fixed_bump_heap";

    if (mb_magic == MULTIBOOT_BOOTLOADER_MAGIC && mb_info != 0) {
        const struct multiboot_info_v1 *mb = (const struct multiboot_info_v1 *)mb_info;
        if ((mb->flags & MB_INFO_FLAG_MMAP) && mb->mmap_addr != 0 && mb->mmap_length > 0) {
            status.map_available = 1;
            uint32_t pos = mb->mmap_addr;
            uint32_t end = mb->mmap_addr + mb->mmap_length;
            while (pos < end) {
                const struct multiboot_mmap_entry *e = (const struct multiboot_mmap_entry *)pos;
                uint32_t type = (e->type == 1u) ? MEM_REGION_USABLE : MEM_REGION_RESERVED;
                if (e->addr_high == 0 && e->len_high == 0)
                    add_region(e->addr_low, e->len_low, type);
                pos += e->size + sizeof(e->size);
            }
        } else if (mb->flags & MB_INFO_FLAG_MEM) {
            uint32_t total = (mb->mem_lower + mb->mem_upper) * 1024u;
            add_region(0, LOW_MEMORY_LIMIT, MEM_REGION_RESERVED);
            if (total > LOW_MEMORY_LIMIT)
                add_region(LOW_MEMORY_LIMIT, total - LOW_MEMORY_LIMIT, MEM_REGION_USABLE);
            status.map_available = 0;
            status.fallback_used = 1;
        }
    }

    if (status.region_count == 0)
        fallback_map();

    if (range_is_usable(status.heap_start, KERNEL_HEAP_BYTES))
        status.heap_end = status.heap_start + KERNEL_HEAP_BYTES;
    else {
        status.heap_start = 0;
        status.allocation_mode = "physical_page_tracker_only";
    }

    page_limit = min_u32(status.total_kib / 4u, MAX_TRACKED_PAGES);
    reserved_low_pages = 0;
    reserved_kernel_pages = 0;
    reserved_heap_pages = 0;
    init_bitmap();

    if (status.map_available) {
        error_log(SEV_INFO, "MEM-000", "Memory",
                  "Multiboot memory map parsed", "run memmap for regions");
    } else {
        error_log(SEV_WARNING, "MEM-001", "Memory",
                  "Multiboot memory map unavailable - using safe fallback",
                  "boot via Multiboot loader with mmap flag");
    }
}

const struct memory_status *memory_status(void)
{
    return &status;
}

void memory_pages(struct page_status *out)
{
    uint32_t used = 0;
    if (!out)
        return;
    for (uint32_t p = 0; p < page_limit; p++) {
        if (bitmap_used(p))
            used++;
    }
    out->page_size = MEMORY_PAGE_SIZE;
    out->total_pages = page_limit;
    out->used_pages = used;
    out->free_pages = page_limit - used;
    out->reserved_low_pages = reserved_low_pages;
    out->reserved_kernel_pages = reserved_kernel_pages;
    out->reserved_heap_pages = reserved_heap_pages;
    out->tracked_limit_kib = MAX_TRACKED_PAGES * 4u;
}

uint32_t memory_region_count(void)
{
    return status.region_count;
}

int memory_region_get(uint32_t index, struct memory_region *out)
{
    if (!out || index >= status.region_count)
        return 0;
    *out = regions[index];
    return 1;
}

const char *memory_region_type_name(uint32_t type)
{
    return type == MEM_REGION_USABLE ? "usable" : "reserved";
}

int memory_map_available(void)
{
    return status.map_available;
}
