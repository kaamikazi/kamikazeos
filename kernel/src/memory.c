/* memory.c - validated Multiboot map parsing and fixed 4 KiB page tracking. */
#include "memory.h"
#include "errors.h"
#include "string.h"

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002u
#define MB_INFO_FLAG_MEM           (1u << 0)
#define MB_INFO_FLAG_MMAP          (1u << 6)
#define MB_MMAP_PAYLOAD_MIN        20u

#define MAX_TRACKED_PAGES     32768u      /* 128 MiB accounting ceiling */
#define PAGE_BITMAP_BYTES     (MAX_TRACKED_PAGES / 8u)
#define TRACKED_BYTES         (MAX_TRACKED_PAGES * MEMORY_PAGE_SIZE)
#define RAW_REGION_MAX        64u
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

struct multiboot_mmap_payload {
    uint32_t addr_low;
    uint32_t addr_high;
    uint32_t len_low;
    uint32_t len_high;
    uint32_t type;
} __attribute__((packed));

struct raw_region {
    uint32_t start;
    uint32_t end; /* exclusive, clipped to the tracked 32-bit range */
    uint32_t type;
};

struct map_parse_result {
    struct raw_region regions[RAW_REGION_MAX];
    uint32_t region_count;
    uint32_t valid_entries;
    uint32_t malformed_entries;
    uint32_t ignored_high_regions;
    uint64_t highest_address;
    uint32_t highest_tracked_end;
    const char *error;
};

extern char _kernel_start;
extern char _kernel_end;

static struct memory_status status;
static struct memory_region regions[MEMORY_MAX_REGIONS];
static struct raw_region raw_regions[RAW_REGION_MAX];
static uint32_t raw_region_count;
static uint8_t page_bitmap[PAGE_BITMAP_BYTES]; /* bit=1 means used/reserved */
static uint32_t page_limit;
static uint32_t reserved_low_pages;
static uint32_t reserved_kernel_pages;
static uint32_t reserved_heap_pages;
static int hardening_tests_passed;

static uint32_t min_u32(uint32_t a, uint32_t b)
{
    return a < b ? a : b;
}

static uint32_t align_down(uint32_t value)
{
    return value & ~(MEMORY_PAGE_SIZE - 1u);
}

static int align_up_checked(uint32_t value, uint32_t *out)
{
    if (!out || value > 0xFFFFFFFFu - (MEMORY_PAGE_SIZE - 1u))
        return 0;
    *out = (value + MEMORY_PAGE_SIZE - 1u) & ~(MEMORY_PAGE_SIZE - 1u);
    return 1;
}

static uint64_t make_u64(uint32_t low, uint32_t high)
{
    return ((uint64_t)high << 32) | (uint64_t)low;
}

static void read_u32(const uint8_t *source, uint32_t *value)
{
    memcpy(value, source, sizeof(*value));
}

static int parse_mmap_buffer(const uint8_t *map, uint32_t length,
                             struct map_parse_result *out)
{
    if (!map || !out || length < sizeof(uint32_t))
        return 0;
    kmemset(out, 0, sizeof(*out));
    out->error = "none";

    uint32_t offset = 0;
    while (offset < length) {
        uint32_t remaining = length - offset;
        uint32_t payload_size = 0;
        if (remaining < sizeof(payload_size)) {
            out->malformed_entries++;
            out->error = "entry_size_truncated";
            return 0;
        }
        read_u32(map + offset, &payload_size);
        if (payload_size < MB_MMAP_PAYLOAD_MIN ||
            payload_size > 0xFFFFFFFFu - sizeof(payload_size)) {
            out->malformed_entries++;
            out->error = "entry_size_invalid";
            return 0;
        }
        uint32_t advance = payload_size + sizeof(payload_size);
        if (advance > remaining) {
            out->malformed_entries++;
            out->error = "entry_exceeds_map_bounds";
            return 0;
        }

        struct multiboot_mmap_payload entry;
        memcpy(&entry, map + offset + sizeof(payload_size), sizeof(entry));
        uint64_t base = make_u64(entry.addr_low, entry.addr_high);
        uint64_t size = make_u64(entry.len_low, entry.len_high);
        uint64_t end = base + size;
        if (size != 0 && end < base) {
            out->malformed_entries++;
            out->error = "physical_endpoint_overflow";
            return 0;
        }
        out->valid_entries++;
        if (end > out->highest_address)
            out->highest_address = end;

        if (size != 0 && base < (uint64_t)TRACKED_BYTES) {
            uint64_t clipped_end64 = end;
            if (clipped_end64 > (uint64_t)TRACKED_BYTES)
                clipped_end64 = (uint64_t)TRACKED_BYTES;
            if (clipped_end64 > base) {
                if (out->region_count >= RAW_REGION_MAX) {
                    out->malformed_entries++;
                    out->error = "raw_region_capacity_exceeded";
                    return 0;
                }
                struct raw_region *region =
                    &out->regions[out->region_count++];
                region->start = (uint32_t)base;
                region->end = (uint32_t)clipped_end64;
                region->type = entry.type == 1u
                    ? MEM_REGION_USABLE : MEM_REGION_RESERVED;
                if (region->end > out->highest_tracked_end)
                    out->highest_tracked_end = region->end;
            }
        } else if (size != 0) {
            out->ignored_high_regions++;
        }
        offset += advance;
    }
    if (offset != length || out->valid_entries == 0) {
        out->error = "empty_or_unconsumed_map";
        return 0;
    }
    return 1;
}

static void bitmap_set_in(uint8_t *bitmap, uint32_t page, uint32_t page_cap)
{
    if (page < page_cap)
        bitmap[page / 8u] |= (uint8_t)(1u << (page % 8u));
}

static void bitmap_clear_in(uint8_t *bitmap, uint32_t page, uint32_t page_cap)
{
    if (page < page_cap)
        bitmap[page / 8u] &= (uint8_t)~(1u << (page % 8u));
}

static int bitmap_used_in(const uint8_t *bitmap, uint32_t page,
                          uint32_t page_cap)
{
    if (page >= page_cap)
        return 1;
    return (bitmap[page / 8u] & (uint8_t)(1u << (page % 8u))) != 0;
}

static uint32_t free_region_pages(uint8_t *bitmap, uint32_t page_cap,
                                  uint32_t start, uint32_t end)
{
    uint32_t aligned_start = 0;
    if (!align_up_checked(start, &aligned_start))
        return 0;
    uint32_t first = aligned_start / MEMORY_PAGE_SIZE;
    uint32_t last = align_down(end) / MEMORY_PAGE_SIZE;
    if (last > page_cap)
        last = page_cap;
    uint32_t changed = 0;
    for (uint32_t page = first; page < last; page++) {
        if (bitmap_used_in(bitmap, page, page_cap))
            changed++;
        bitmap_clear_in(bitmap, page, page_cap);
    }
    return changed;
}

static uint32_t reserve_region_pages(uint8_t *bitmap, uint32_t page_cap,
                                     uint32_t start, uint32_t end)
{
    if (end <= start)
        return 0;
    uint32_t aligned_end = 0;
    if (!align_up_checked(end, &aligned_end))
        aligned_end = TRACKED_BYTES;
    uint32_t first = align_down(start) / MEMORY_PAGE_SIZE;
    uint32_t last = aligned_end / MEMORY_PAGE_SIZE;
    if (last > page_cap)
        last = page_cap;
    uint32_t changed = 0;
    for (uint32_t page = first; page < last; page++) {
        if (!bitmap_used_in(bitmap, page, page_cap))
            changed++;
        bitmap_set_in(bitmap, page, page_cap);
    }
    return changed;
}

static uint32_t region_page_count(uint32_t page_cap, uint32_t start,
                                  uint32_t end)
{
    if (end <= start)
        return 0;
    uint32_t aligned_end = 0;
    if (!align_up_checked(end, &aligned_end))
        aligned_end = page_cap * MEMORY_PAGE_SIZE;
    uint32_t first = align_down(start) / MEMORY_PAGE_SIZE;
    uint32_t last = min_u32(aligned_end / MEMORY_PAGE_SIZE, page_cap);
    return last > first ? last - first : 0;
}

static void apply_raw_regions(uint8_t *bitmap, uint32_t page_cap,
                              const struct raw_region *map, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++)
        if (map[i].type == MEM_REGION_USABLE)
            free_region_pages(bitmap, page_cap, map[i].start, map[i].end);
    /* Reserved entries win over every overlapping usable entry. */
    for (uint32_t i = 0; i < count; i++)
        if (map[i].type != MEM_REGION_USABLE)
            reserve_region_pages(bitmap, page_cap, map[i].start, map[i].end);
}

static int range_is_free(uint32_t start, uint32_t end)
{
    if (end <= start || end > page_limit * MEMORY_PAGE_SIZE)
        return 0;
    uint32_t aligned_start = 0;
    uint32_t aligned_end = 0;
    if (!align_up_checked(start, &aligned_start) ||
        !align_up_checked(end, &aligned_end))
        return 0;
    for (uint32_t page = aligned_start / MEMORY_PAGE_SIZE;
         page < aligned_end / MEMORY_PAGE_SIZE; page++)
        if (bitmap_used_in(page_bitmap, page, page_limit))
            return 0;
    return 1;
}

static int range_is_reserved(uint32_t start, uint32_t end)
{
    if (end <= start)
        return 1;
    uint32_t aligned_end = 0;
    if (!align_up_checked(end, &aligned_end))
        aligned_end = page_limit * MEMORY_PAGE_SIZE;
    uint32_t first = align_down(start) / MEMORY_PAGE_SIZE;
    uint32_t last = min_u32(aligned_end / MEMORY_PAGE_SIZE, page_limit);
    for (uint32_t page = first; page < last; page++)
        if (!bitmap_used_in(page_bitmap, page, page_limit))
            return 0;
    return 1;
}

static void build_normalized_regions(void)
{
    kmemset(regions, 0, sizeof(regions));
    status.region_count = 0;
    status.total_kib = page_limit * (MEMORY_PAGE_SIZE / 1024u);
    status.usable_kib = 0;
    status.reserved_kib = 0;
    if (page_limit == 0)
        return;

    uint32_t run_start = 0;
    int run_used = bitmap_used_in(page_bitmap, 0, page_limit);
    for (uint32_t page = 1; page <= page_limit; page++) {
        int used = page == page_limit ? !run_used
                                      : bitmap_used_in(page_bitmap, page,
                                                       page_limit);
        if (page != page_limit && used == run_used)
            continue;
        uint32_t length = (page - run_start) * MEMORY_PAGE_SIZE;
        if (status.region_count < MEMORY_MAX_REGIONS) {
            struct memory_region *region = &regions[status.region_count++];
            region->base = run_start * MEMORY_PAGE_SIZE;
            region->length = length;
            region->type = run_used ? MEM_REGION_RESERVED : MEM_REGION_USABLE;
        }
        if (run_used)
            status.reserved_kib += length / 1024u;
        else
            status.usable_kib += length / 1024u;
        run_start = page;
        run_used = used;
    }
    status.normalized = 1;
}

static int bitmap_consistency_check(void)
{
    if (!range_is_reserved(0, min_u32(LOW_MEMORY_LIMIT,
                                      page_limit * MEMORY_PAGE_SIZE)))
        return 0;
    if (!range_is_reserved((uint32_t)&_kernel_start,
                           (uint32_t)&_kernel_end))
        return 0;
    if (!range_is_reserved(0x000B8000u, 0x000B9000u))
        return 0;
    if (status.heap_end > status.heap_start &&
        !range_is_reserved(status.heap_start, status.heap_end))
        return 0;
    for (uint32_t i = 0; i < raw_region_count; i++)
        if (raw_regions[i].type != MEM_REGION_USABLE &&
            !range_is_reserved(raw_regions[i].start, raw_regions[i].end))
            return 0;
    for (uint32_t page = page_limit; page < MAX_TRACKED_PAGES; page++)
        if (!bitmap_used_in(page_bitmap, page, MAX_TRACKED_PAGES))
            return 0;
    return 1;
}

static void install_parse_result(const struct map_parse_result *parsed)
{
    raw_region_count = parsed->region_count;
    memcpy(raw_regions, parsed->regions,
           raw_region_count * sizeof(raw_regions[0]));
    status.raw_region_count = raw_region_count;
    status.malformed_entries = parsed->malformed_entries;
    status.ignored_high_regions = parsed->ignored_high_regions;
    status.highest_address_low = (uint32_t)parsed->highest_address;
    status.highest_address_high = (uint32_t)(parsed->highest_address >> 32);
    status.tracked_end = parsed->highest_tracked_end;
    status.validation_error = parsed->error;
    uint32_t tracked_end = min_u32(parsed->highest_tracked_end, TRACKED_BYTES);
    uint32_t aligned_end = 0;
    if (!align_up_checked(tracked_end, &aligned_end))
        aligned_end = TRACKED_BYTES;
    page_limit = min_u32(aligned_end / MEMORY_PAGE_SIZE, MAX_TRACKED_PAGES);
}

static void install_fallback(uint32_t bytes, const char *reason)
{
    struct map_parse_result fallback;
    kmemset(&fallback, 0, sizeof(fallback));
    if (bytes < LOW_MEMORY_LIMIT)
        bytes = FALLBACK_MEMORY_BYTES;
    if (bytes > TRACKED_BYTES)
        bytes = TRACKED_BYTES;
    fallback.regions[0].start = 0;
    fallback.regions[0].end = min_u32(LOW_MEMORY_LIMIT, bytes);
    fallback.regions[0].type = MEM_REGION_RESERVED;
    fallback.region_count = 1;
    if (bytes > LOW_MEMORY_LIMIT) {
        fallback.regions[1].start = LOW_MEMORY_LIMIT;
        fallback.regions[1].end = bytes;
        fallback.regions[1].type = MEM_REGION_USABLE;
        fallback.region_count++;
    }
    fallback.valid_entries = fallback.region_count;
    fallback.highest_address = bytes;
    fallback.highest_tracked_end = bytes;
    fallback.error = reason;
    install_parse_result(&fallback);
    status.map_available = 0;
    status.fallback_used = 1;
    status.map_validated = 0;
}

static int memory_hardening_tests(void)
{
    struct test_entry {
        uint32_t size;
        struct multiboot_mmap_payload payload;
    } __attribute__((packed));
    struct test_entry entries[3];
    struct map_parse_result parsed;
    uint8_t bitmap[2048u / 8u];
    kmemset(entries, 0, sizeof(entries));
    for (uint32_t i = 0; i < 3; i++)
        entries[i].size = MB_MMAP_PAYLOAD_MIN;
    entries[0].payload.len_low = LOW_MEMORY_LIMIT;
    entries[0].payload.type = 2;
    entries[1].payload.addr_low = LOW_MEMORY_LIMIT;
    entries[1].payload.len_low = 7u * 1024u * 1024u;
    entries[1].payload.type = 1;
    entries[2].payload.addr_low = 2u * 1024u * 1024u;
    entries[2].payload.len_low = 1u * 1024u * 1024u;
    entries[2].payload.type = 2;
    if (!parse_mmap_buffer((const uint8_t *)entries, sizeof(entries),
                           &parsed) || parsed.region_count != 3)
        return 0;
    kmemset(bitmap, 0xFF, sizeof(bitmap));
    apply_raw_regions(bitmap, 2048u, parsed.regions, parsed.region_count);
    if (bitmap_used_in(bitmap, 256u, 2048u) ||
        !bitmap_used_in(bitmap, 512u, 2048u) ||
        bitmap_used_in(bitmap, 768u, 2048u))
        return 0;

    entries[0].size = MB_MMAP_PAYLOAD_MIN - 4u;
    if (parse_mmap_buffer((const uint8_t *)entries, sizeof(entries[0]),
                          &parsed))
        return 0;
    entries[0].size = MB_MMAP_PAYLOAD_MIN;
    if (parse_mmap_buffer((const uint8_t *)entries,
                          sizeof(entries[0]) - 1u, &parsed))
        return 0;

    entries[0].payload.addr_low = 0xF0000000u;
    entries[0].payload.addr_high = 0xFFFFFFFFu;
    entries[0].payload.len_low = 0x20000000u;
    entries[0].payload.len_high = 0;
    if (parse_mmap_buffer((const uint8_t *)entries, sizeof(entries[0]),
                          &parsed))
        return 0;

    entries[0].payload.addr_low = 0;
    entries[0].payload.addr_high = 1;
    entries[0].payload.len_low = MEMORY_PAGE_SIZE;
    entries[0].payload.len_high = 0;
    return parse_mmap_buffer((const uint8_t *)entries, sizeof(entries[0]),
                             &parsed) && parsed.region_count == 0 &&
           parsed.ignored_high_regions == 1 &&
           (uint32_t)(parsed.highest_address >> 32) == 1u;
}

void memory_init(uint32_t mb_magic, uint32_t mb_info)
{
    kmemset(&status, 0, sizeof(status));
    kmemset(regions, 0, sizeof(regions));
    kmemset(raw_regions, 0, sizeof(raw_regions));
    kmemset(page_bitmap, 0xFF, sizeof(page_bitmap));
    raw_region_count = 0;
    page_limit = 0;
    status.kernel_start = (uint32_t)&_kernel_start;
    status.kernel_end = (uint32_t)&_kernel_end;
    status.heap_start = 0;
    status.heap_end = 0;
    status.allocation_mode = "physical_page_tracker_plus_fixed_bump_heap";
    status.validation_error = "multiboot_map_unavailable";
    hardening_tests_passed = memory_hardening_tests();
    status.hardening_self_test = hardening_tests_passed;

    int installed_map = 0;
    int map_rejected = 0;
    uint32_t rejected_malformed_entries = 0;
    const char *fallback_reason = status.validation_error;
    if (mb_magic == MULTIBOOT_BOOTLOADER_MAGIC && mb_info != 0) {
        const struct multiboot_info_v1 *mb =
            (const struct multiboot_info_v1 *)mb_info;
        if ((mb->flags & MB_INFO_FLAG_MMAP) && mb->mmap_addr != 0 &&
            mb->mmap_length > 0 &&
            mb->mmap_addr <= 0xFFFFFFFFu - mb->mmap_length) {
            struct map_parse_result parsed;
            if (parse_mmap_buffer((const uint8_t *)mb->mmap_addr,
                                  mb->mmap_length, &parsed)) {
                install_parse_result(&parsed);
                status.map_available = 1;
                status.map_validated = 1;
                installed_map = 1;
            } else {
                map_rejected = 1;
                rejected_malformed_entries = parsed.malformed_entries;
                fallback_reason = parsed.error;
            }
        }
        if (!installed_map && (mb->flags & MB_INFO_FLAG_MEM)) {
            uint32_t bytes = FALLBACK_MEMORY_BYTES;
            if (mb->mem_upper <=
                (0xFFFFFFFFu - LOW_MEMORY_LIMIT) / 1024u)
                bytes = LOW_MEMORY_LIMIT + mb->mem_upper * 1024u;
            if (!map_rejected)
                fallback_reason = "multiboot_basic_memory_fallback";
            install_fallback(bytes, fallback_reason);
            status.malformed_entries = rejected_malformed_entries;
            installed_map = 1;
        }
    }
    if (!installed_map) {
        install_fallback(FALLBACK_MEMORY_BYTES, fallback_reason);
        status.malformed_entries = rejected_malformed_entries;
    }

    kmemset(page_bitmap, 0xFF, sizeof(page_bitmap));
    apply_raw_regions(page_bitmap, page_limit, raw_regions, raw_region_count);

    uint32_t heap_start = 0;
    if (align_up_checked(status.kernel_end, &heap_start) &&
        heap_start <= 0xFFFFFFFFu - KERNEL_HEAP_BYTES &&
        range_is_free(heap_start, heap_start + KERNEL_HEAP_BYTES)) {
        status.heap_start = heap_start;
        status.heap_end = heap_start + KERNEL_HEAP_BYTES;
    } else {
        status.allocation_mode = "physical_page_tracker_only";
    }

    uint32_t low_end = min_u32(LOW_MEMORY_LIMIT,
                               page_limit * MEMORY_PAGE_SIZE);
    reserve_region_pages(page_bitmap, page_limit, 0, low_end);
    reserve_region_pages(page_bitmap, page_limit,
                         status.kernel_start, status.kernel_end);
    if (status.heap_end > status.heap_start)
        reserve_region_pages(page_bitmap, page_limit,
                             status.heap_start, status.heap_end);
    reserved_low_pages = region_page_count(page_limit, 0, low_end);
    reserved_kernel_pages = region_page_count(
        page_limit, status.kernel_start, status.kernel_end);
    reserved_heap_pages = status.heap_end > status.heap_start
        ? region_page_count(page_limit, status.heap_start, status.heap_end) : 0;

    status.critical_ranges_reserved =
        range_is_reserved(0, low_end) &&
        range_is_reserved(0x000B8000u, 0x000B9000u) &&
        range_is_reserved(status.kernel_start, status.kernel_end) &&
        (status.heap_end <= status.heap_start ||
         range_is_reserved(status.heap_start, status.heap_end));
    status.bitmap_consistent = status.critical_ranges_reserved &&
                               bitmap_consistency_check();
    build_normalized_regions();

    if (!hardening_tests_passed || !status.bitmap_consistent) {
        error_log(SEV_ERROR, "MEM-102", "Memory",
                  "Memory map defensive validation failed",
                  "do not allocate physical or user pages");
    } else if (status.map_validated) {
        error_log(SEV_INFO, "MEM-000", "Memory",
                  "Multiboot memory map validated and normalized",
                  "reserved ranges override usable ranges");
    } else {
        error_log(SEV_WARNING, "MEM-001", "Memory",
                  "Multiboot map rejected or unavailable; safe fallback active",
                  status.validation_error);
    }
}

const struct memory_status *memory_status(void)
{
    return &status;
}

void memory_pages(struct page_status *out)
{
    if (!out)
        return;
    uint32_t used = 0;
    for (uint32_t page = 0; page < page_limit; page++)
        if (bitmap_used_in(page_bitmap, page, page_limit))
            used++;
    out->page_size = MEMORY_PAGE_SIZE;
    out->total_pages = page_limit;
    out->used_pages = used;
    out->free_pages = page_limit - used;
    out->reserved_low_pages = reserved_low_pages;
    out->reserved_kernel_pages = reserved_kernel_pages;
    out->reserved_heap_pages = reserved_heap_pages;
    out->tracked_limit_kib = page_limit * (MEMORY_PAGE_SIZE / 1024u);
    out->bitmap_consistent = status.bitmap_consistent;
    out->critical_ranges_reserved = status.critical_ranges_reserved;
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
    return status.map_available && status.map_validated;
}

int memory_hardening_self_test(void)
{
    return hardening_tests_passed;
}
