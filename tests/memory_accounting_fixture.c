/* Production accounting/policy with synthetic maps, no port/privileged I/O. */
#include "memory_policy_fixture.c"
#include "../kernel/src/memory_accounting.h"

static int account(const struct mp_layout *l, const struct mp_ram_region *map, uint32_t n,
                   const struct mp_reservation *r, uint32_t rn, uint32_t cap,
                   int trusted, int active, struct mp_ram_accounting *out)
{ return mp_account_ram(l,map,n,r,rn,cap,trusted,active,out); }

int test_ram_union_reserved_wins_without_double_counting(void)
{
    struct mp_layout l=layout();struct mp_ram_accounting a;
    const struct mp_ram_region map[]={
        {0x200000,0x300000,0},{0,0x4000000,1},{0x100000,0x400000,1}};
    if (!account(&l,map,3,0,0,0x4000000,1,1,&a)) return 0;
    return a.detected_usable_bytes==0x3F00000 && a.tracked_usable_pages==0x3F00000/4096 &&
        a.mapped_usable_pages==0xF00000/4096 && a.mapped_nonram_pages==256 &&
        a.eligible_pages==(MP_APERTURE_END-l.regions[MP_HEAP].end-0x100000)/4096;
}
int test_high_ram_discovery_is_separate_from_tracking_and_mapping(void)
{
    struct mp_layout l=layout();struct mp_ram_accounting a;
    const struct mp_ram_region map[]={{0,0x10000000,1},{0x100000000ULL,0x100800000ULL,1}};
    if (!account(&l,map,2,0,0,MP_TRACKED_END,1,1,&a)) return 0;
    return a.detected_usable_bytes==0x10800000 && a.tracked_usable_pages==MP_TRACKED_END/4096 &&
        a.mapped_present_pages==4096 && a.mapped_usable_pages==4096 &&
        a.unmapped_usable_pages==(MP_TRACKED_END-MP_APERTURE_END)/4096 &&
        a.eligible_pages==(MP_APERTURE_END-l.regions[MP_HEAP].end)/4096;
}
int test_partial_adjacent_and_reserved_pages(void)
{
    struct mp_ram_region map[]={{0x400000,0x400800,1},{0x400800,0x401000,1},{0,0,0}};
    if (!mp_ram_page_usable(map,3,0x400000)) return 0;
    map[2]=(struct mp_ram_region){0x400FFF,0x401000,0};
    if (mp_ram_page_usable(map,3,0x400000)) return 0;
    map[2]=(struct mp_ram_region){0,0,0};map[0].start++;
    return !mp_ram_page_usable(map,3,0x400000) && !mp_ram_page_usable(map,3,0x400001) &&
        !mp_ram_page_usable(map,3,UINT64_MAX-4095);
}
int test_untrusted_fallback_never_claims_detected_or_eligible_ram(void)
{
    struct mp_layout l=layout();struct mp_ram_accounting a;
    const struct mp_ram_region map[]={{0,MP_APERTURE_END,1}};
    return account(&l,map,1,0,0,MP_APERTURE_END,0,1,&a) && a.valid && !a.discovery_trusted &&
        a.described_usable_bytes==MP_APERTURE_END && !a.detected_usable_bytes &&
        !a.eligible_pages && a.mapped_present_pages==4096;
}
int test_disabled_mapping_does_not_claim_accessible_frames(void)
{
    struct mp_layout l=layout();struct mp_ram_accounting a;
    const struct mp_ram_region map[]={{0,0x4000000,1}};
    return account(&l,map,1,0,0,0x4000000,1,0,&a) && !a.mapped_present_pages &&
        !a.mapped_usable_pages && !a.eligible_pages && a.unmapped_usable_pages==a.tracked_usable_pages;
}
int test_boot_reservations_round_outward_and_do_not_double_count(void)
{
    struct mp_layout l=layout();struct mp_ram_accounting a,b;
    const struct mp_ram_region map[]={{0,MP_APERTURE_END,1}};
    uint64_t start=l.regions[MP_HEAP].end;
    const struct mp_reservation reservations[]={{start+1,start+4097},{start+2,start+4096}};
    if (!account(&l,map,1,0,0,MP_APERTURE_END,1,1,&a) ||
        !account(&l,map,1,reservations,2,MP_APERTURE_END,1,1,&b)) return 0;
    return b.eligible_pages+2==a.eligible_pages && b.reserved_usable_pages==a.reserved_usable_pages+2;
}
int test_inactive_heap_candidate_is_not_an_allocated_heap_reservation(void)
{
    struct mp_layout l=layout();struct mp_ram_accounting a,b;
    const struct mp_ram_region map[]={{0,MP_APERTURE_END,1}};
    if (!account(&l,map,1,0,0,MP_APERTURE_END,1,1,&a)) return 0;
    l.heap_available=0;
    return account(&l,map,1,0,0,MP_APERTURE_END,1,1,&b) &&
        b.eligible_pages==a.eligible_pages+MP_HEAP_BYTES/4096;
}
int test_invalid_accounting_inputs_fail_closed(void)
{
    struct mp_layout l=layout();struct mp_ram_accounting a;
    struct mp_ram_region map[]={{0,MP_APERTURE_END,1}};
    if (account(&l,map,1,0,0,MP_TRACKED_END+4096,1,1,&a) || a.eligible_pages) return 0;
    if (account(&l,map,1,0,0,MP_TRACKED_END-1,1,1,&a) || a.eligible_pages) return 0;
    if (account(&l,map,MP_RAM_MAX+1,0,0,MP_TRACKED_END,1,1,&a)) return 0;
    if (account(&l,map,1,0,1,MP_TRACKED_END,1,1,&a)) return 0;
    if (account(&l,map,1,0,0,MP_TRACKED_END,2,1,&a)) return 0;
    map[0].end=0;map[0].start=4096;
    return !account(&l,map,1,0,0,MP_TRACKED_END,1,1,&a) && !a.eligible_pages;
}
int test_small_ram_holes_and_bounded_eligibility(void)
{
    struct mp_layout l=layout();struct mp_ram_accounting a;
    const uint32_t sizes[]={4,8,16,32,64,128,256};
    for (uint32_t i=0;i<7;i++) {
        uint32_t end=sizes[i]*0x100000u;
        struct mp_ram_region map[]={{0,end,1},{0xA0000,0x100000,0}};
        uint32_t cap=end<MP_TRACKED_END?end:MP_TRACKED_END;
        if (!account(&l,map,2,0,0,cap,1,1,&a)) return 0;
        uint32_t accessible=end<MP_APERTURE_END?end:MP_APERTURE_END;
        if (a.detected_usable_bytes!=(uint64_t)end-0x60000 ||
            a.eligible_pages!=(accessible-l.regions[MP_HEAP].end)/4096 ||
            a.mapped_usable_pages!=(accessible-0x60000)/4096) return 0;
    }
    return 1;
}
