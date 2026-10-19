/* Phase A shadow accounting: describes eligibility, never allocates frames. */
#ifndef KMZ_MEMORY_ACCOUNTING_H
#define KMZ_MEMORY_ACCOUNTING_H
#include "memory_policy.h"
#define MP_RAM_MAX 64u
#define MP_RESERVATION_MAX 8u
#define MP_TRACKED_END 0x08000000u
struct mp_ram_region { uint64_t start, end; uint32_t usable; };
struct mp_reservation { uint64_t start, end; };
struct mp_ram_accounting {
    uint64_t described_usable_bytes; /* reserved-wins union, including high RAM */
    uint64_t detected_usable_bytes;  /* zero if discovery is untrusted */
    uint32_t valid, discovery_trusted, mapping_active;
    uint32_t tracked_usable_pages, mapped_present_pages, mapped_usable_pages;
    uint32_t mapped_nonram_pages, reserved_usable_pages, unmapped_usable_pages;
    uint32_t eligible_pages;
    const char *reason;
};
int mp_ram_page_usable(const struct mp_ram_region *map, uint32_t count, uint64_t page);
int mp_account_ram(const struct mp_layout *layout,
                   const struct mp_ram_region *map, uint32_t count,
                   const struct mp_reservation *extra, uint32_t extra_count,
                   uint32_t tracked_end, int trusted, int mapping_active,
                   struct mp_ram_accounting *out);
#endif
