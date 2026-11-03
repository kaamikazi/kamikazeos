#include "memory_policy.h"

static const char *const names[MP_REGION_COUNT] = {
    "null", "legacy_low_before_vga", "vga", "legacy_low_after_vga",
    "text", "rodata", "data", "bss", "page_directory", "page_tables",
    "bootstrap_guard_low", "bootstrap_stack", "bootstrap_guard_high",
    "privilege_guard_low", "privilege_stack", "privilege_guard_high",
    "fatal_guard_low", "fatal_stack", "fatal_guard_high",
    "df_guard_low", "df_stack", "df_guard_high",
    "user_code", "user_guard_low", "user_stack", "user_guard_high", "heap_candidate"
};

const char *mp_region_name(uint32_t id)
{ return id < MP_REGION_COUNT ? names[id] : "unclassified"; }

static int fail(const char **why, const char *reason)
{ if (why) *why=reason; return 0; }

int mp_layout_validate(const struct mp_layout *l, const char **why)
{
    if (!l) return fail(why,"missing_layout");
    if (l->heap_available > 1) return fail(why,"invalid_heap_state");
    for (uint32_t i=0;i<MP_REGION_COUNT;i++) {
        const struct mp_span *s=&l->regions[i];
        if (s->end<=s->start || s->end>MP_APERTURE_END)
            return fail(why,"empty_wrapped_or_outside_aperture");
        if ((s->start|s->end)&(MP_PAGE_SIZE-1u)) return fail(why,"unaligned_span");
        if (i && s->start!=l->regions[i-1].end) return fail(why,"gap_overlap_or_order");
    }
    if (l->regions[MP_NULL].start!=0 || l->regions[MP_NULL].end!=MP_PAGE_SIZE ||
        l->regions[MP_VGA].start!=0xB8000u || l->regions[MP_VGA].end!=0xB9000u ||
        l->regions[MP_TEXT].start!=0x100000u) return fail(why,"fixed_low_layout");
    for (uint32_t i=0;i<MP_REGION_COUNT;i++) {
        uint32_t size=l->regions[i].end-l->regions[i].start, required=0;
        switch (i) {
        case MP_DIRECTORY: case MP_USER_CODE:
        case MP_BOOT_GUARD_LOW: case MP_BOOT_GUARD_HIGH:
        case MP_PRIV_GUARD_LOW: case MP_PRIV_GUARD_HIGH:
        case MP_USER_GUARD_LOW: case MP_USER_GUARD_HIGH:
        case MP_FATAL_GUARD_LOW: case MP_FATAL_GUARD_HIGH:
        case MP_DF_GUARD_LOW: case MP_DF_GUARD_HIGH: required=MP_PAGE_SIZE; break;
        case MP_TABLES: case MP_BOOT_STACK: case MP_PRIV_STACK: case MP_USER_STACK:
        case MP_FATAL_STACK: case MP_DF_STACK:
            required=4u*MP_PAGE_SIZE; break;
        case MP_HEAP: required=MP_HEAP_BYTES; break;
        }
        if (required && size!=required) return fail(why,"fixed_region_size");
    }
    if (why) *why="none";
    return 1;
}

int mp_region_at(const struct mp_layout *l, uint32_t address)
{
    if (!l) return -1;
    for (uint32_t i=0;i<MP_REGION_COUNT;i++)
        if (address>=l->regions[i].start && address<l->regions[i].end) return (int)i;
    return -1;
}

uint32_t mp_page_flags(const struct mp_layout *l, uint32_t address,
                       enum mp_policy_mode mode, int ram_usable)
{
    if (!mp_layout_validate(l,0) || address>=MP_APERTURE_END) return 0;
    int id=mp_region_at(l,address);
    uint32_t flags=MP_PRESENT|MP_WRITABLE;
    if (mode==MP_LEGACY_ACTIVE) {
        if (id==MP_USER_CODE) return MP_PRESENT|MP_USER;
        if (id==MP_USER_STACK) return flags|MP_USER;
        return flags; /* NULL, reserved holes and guards remain mapped in A. */
    }
    if (mode!=MP_PROTECTED_PLAN) return 0;
    switch (id) {
    case MP_NULL: case MP_LOW_BEFORE_VGA: case MP_LOW_AFTER_VGA:
    case MP_BOOT_GUARD_LOW: case MP_BOOT_GUARD_HIGH:
    case MP_PRIV_GUARD_LOW: case MP_PRIV_GUARD_HIGH:
    case MP_USER_GUARD_LOW: case MP_USER_GUARD_HIGH:
    case MP_FATAL_GUARD_LOW: case MP_FATAL_GUARD_HIGH:
    case MP_DF_GUARD_LOW: case MP_DF_GUARD_HIGH: return 0;
    case MP_VGA: return flags;
    }
    if (!ram_usable || (id==MP_HEAP && !l->heap_available)) return 0;
    switch (id) {
    case MP_TEXT: case MP_RODATA: case MP_DIRECTORY: case MP_TABLES: return MP_PRESENT;
    case MP_USER_CODE: return MP_PRESENT|MP_USER;
    case MP_USER_STACK: return flags|MP_USER;
    default: return flags;
    }
}

int mp_region_is_guard(uint32_t id)
{
    switch(id) {
    case MP_BOOT_GUARD_LOW: case MP_BOOT_GUARD_HIGH: case MP_PRIV_GUARD_LOW: case MP_PRIV_GUARD_HIGH:
    case MP_FATAL_GUARD_LOW: case MP_FATAL_GUARD_HIGH: case MP_DF_GUARD_LOW: case MP_DF_GUARD_HIGH:
    case MP_USER_GUARD_LOW: case MP_USER_GUARD_HIGH: return 1;
    default:return 0;
    }
}
uint32_t mp_guard_pages(const struct mp_layout *l)
{
    if(!mp_layout_validate(l,0))return 0;
    uint32_t pages=0;
    for(uint32_t i=0;i<MP_REGION_COUNT;i++)if(mp_region_is_guard(i))
        pages+=(l->regions[i].end-l->regions[i].start)/MP_PAGE_SIZE;
    return pages;
}
