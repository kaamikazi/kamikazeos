/* Actual pure policy C, with synthetic linker-style spans; no CPU/I/O mocks. */
#include "../kernel/src/memory_policy.h"

static struct mp_layout layout(void)
{
    struct mp_layout l={0};
    l.regions[MP_NULL]=(struct mp_span){0,4096};
    l.regions[MP_LOW_BEFORE_VGA]=(struct mp_span){4096,0xB8000};
    l.regions[MP_VGA]=(struct mp_span){0xB8000,0xB9000};
    l.regions[MP_LOW_AFTER_VGA]=(struct mp_span){0xB9000,0x100000};
    uint32_t at=0x100000;
    for (int i=MP_TEXT;i<MP_REGION_COUNT;i++) {
        uint32_t size=4096;
        if (i==MP_TABLES || i==MP_BOOT_STACK || i==MP_PRIV_STACK || i==MP_USER_STACK) size=16384;
        if (i==MP_HEAP) size=65536;
        l.regions[i]=(struct mp_span){at,at+size};at+=size;
    }
    l.heap_available=1;
    return l;
}
int test_valid_layout_and_exclusive_boundaries(void)
{
    struct mp_layout l=layout();
    return mp_layout_validate(&l,0) && mp_region_at(&l,l.regions[MP_USER_CODE].end)==MP_USER_GUARD_LOW &&
        mp_region_at(&l,l.regions[MP_HEAP].end)==-1;
}
int test_unaligned_empty_and_wrapped_spans_are_rejected(void)
{
    for (int i=0;i<MP_REGION_COUNT;i++) {
        struct mp_layout l=layout();l.regions[i].start++;
        if (mp_layout_validate(&l,0) || mp_page_flags(&l,0,MP_LEGACY_ACTIVE,1)) return 0;
        l=layout();l.regions[i].end=l.regions[i].start;
        if (mp_layout_validate(&l,0)) return 0;
        l=layout();l.regions[i].start=0xFFFFF000;l.regions[i].end=0;
        if (mp_layout_validate(&l,0)) return 0;
    }
    return 1;
}
int test_gap_overlap_and_reordered_spans_are_rejected(void)
{
    for (int i=MP_TEXT;i<MP_REGION_COUNT;i++) {
        struct mp_layout l=layout();l.regions[i].start-=4096;
        if (mp_layout_validate(&l,0)) return 0;
        l=layout();l.regions[i].start+=4096;l.regions[i].end+=4096;
        if (mp_layout_validate(&l,0)) return 0;
    }
    struct mp_layout l=layout();struct mp_span t=l.regions[MP_TEXT];
    l.regions[MP_TEXT]=l.regions[MP_RODATA];l.regions[MP_RODATA]=t;
    return !mp_layout_validate(&l,0);
}
int test_outside_aperture_and_wrong_fixed_sizes_are_rejected(void)
{
    struct mp_layout l=layout();l.regions[MP_HEAP].end=MP_APERTURE_END+4096;
    if (mp_layout_validate(&l,0)) return 0;
    l=layout();l.regions[MP_USER_CODE].end+=4096;
    for (int i=MP_USER_CODE+1;i<MP_REGION_COUNT;i++) {l.regions[i].start+=4096;l.regions[i].end+=4096;}
    return !mp_layout_validate(&l,0);
}
int test_legacy_policy_matches_every_page_and_exactly_five_user_pages(void)
{
    struct mp_layout l=layout();uint32_t users=0;
    for (uint32_t p=0;p<MP_APERTURE_END;p+=4096) {
        uint32_t expected=MP_PRESENT|MP_WRITABLE;
        if (p>=l.regions[MP_USER_CODE].start && p<l.regions[MP_USER_CODE].end) expected=MP_PRESENT|MP_USER;
        if (p>=l.regions[MP_USER_STACK].start && p<l.regions[MP_USER_STACK].end) expected|=MP_USER;
        uint32_t actual=mp_page_flags(&l,p,MP_LEGACY_ACTIVE,0);
        if (actual!=expected) return 0;
        if (actual&MP_USER) users++;
    }
    return users==5 && !mp_page_flags(&l,MP_APERTURE_END,MP_LEGACY_ACTIVE,1);
}
int test_planned_section_permissions_do_not_imply_nx(void)
{
    struct mp_layout l=layout();
    for (int i=MP_TEXT;i<MP_REGION_COUNT;i++) {
        uint32_t expected=3;
        switch (i) {
        case MP_TEXT: case MP_RODATA: case MP_DIRECTORY: case MP_TABLES: expected=1;break;
        case MP_USER_CODE:expected=5;break;case MP_USER_STACK:expected=7;break;
        case MP_BOOT_GUARD_LOW:case MP_BOOT_GUARD_HIGH:case MP_PRIV_GUARD_LOW:
        case MP_PRIV_GUARD_HIGH:case MP_USER_GUARD_LOW:case MP_USER_GUARD_HIGH:expected=0;break;
        }
        if (mp_page_flags(&l,l.regions[i].start,MP_PROTECTED_PLAN,1)!=expected) return 0;
    }
    return 1; /* Only P/RW/USER flags: no claimed non-PAE execute-disable bit. */
}
int test_guard_reservations_remain_present_under_active_policy(void)
{
    struct mp_layout l=layout();int count=0;
    const int guards[]={MP_BOOT_GUARD_LOW,MP_BOOT_GUARD_HIGH,MP_PRIV_GUARD_LOW,
        MP_PRIV_GUARD_HIGH,MP_USER_GUARD_LOW,MP_USER_GUARD_HIGH};
    for (int i=0;i<6;i++) {
        uint32_t a=l.regions[guards[i]].start;
        if (mp_page_flags(&l,a,MP_LEGACY_ACTIVE,1)!=3 || mp_page_flags(&l,a,MP_PROTECTED_PLAN,1)) return 0;
        count++;
    }
    return count==6 && mp_page_flags(&l,0,MP_LEGACY_ACTIVE,1)==3 &&
        !mp_page_flags(&l,0,MP_PROTECTED_PLAN,1);
}
int test_planned_ram_device_and_heap_availability_rules(void)
{
    struct mp_layout l=layout();
    if (mp_page_flags(&l,l.regions[MP_TEXT].start,MP_PROTECTED_PLAN,0) ||
        mp_page_flags(&l,0xB8000,MP_PROTECTED_PLAN,0)!=3 ||
        mp_page_flags(&l,0x800000,MP_PROTECTED_PLAN,0) ||
        mp_page_flags(&l,0x800000,MP_PROTECTED_PLAN,1)!=3) return 0;
    l.heap_available=0;
    return !mp_page_flags(&l,l.regions[MP_HEAP].start,MP_PROTECTED_PLAN,1) &&
        mp_page_flags(&l,l.regions[MP_HEAP].start,MP_LEGACY_ACTIVE,1)==3;
}
int test_bad_modes_and_missing_layout_fail_closed(void)
{
    struct mp_layout l=layout();
    return !mp_layout_validate(0,0) && !mp_page_flags(0,0,MP_LEGACY_ACTIVE,1) &&
        !mp_page_flags(&l,0x100000,(enum mp_policy_mode)99,1) && mp_region_at(0,0)==-1;
}
