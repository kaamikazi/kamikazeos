/* Phase A: descriptive policy, not a mapping/protection activation API. */
#ifndef KMZ_MEMORY_POLICY_H
#define KMZ_MEMORY_POLICY_H
#include <stdint.h>

#define MP_PAGE_SIZE       4096u
#define MP_APERTURE_END    0x01000000u
#define MP_HEAP_BYTES      0x00010000u
#define MP_PRESENT        0x001u
#define MP_WRITABLE       0x002u
#define MP_USER           0x004u

/* Primary, non-overlapping, physically ordered spans. */
enum mp_region_id {
    MP_NULL, MP_LOW_BEFORE_VGA, MP_VGA, MP_LOW_AFTER_VGA,
    MP_TEXT, MP_RODATA, MP_DATA, MP_BSS, MP_DIRECTORY, MP_TABLES,
    MP_BOOT_GUARD_LOW, MP_BOOT_STACK, MP_BOOT_GUARD_HIGH,
    MP_PRIV_GUARD_LOW, MP_PRIV_STACK, MP_PRIV_GUARD_HIGH,
    MP_USER_CODE, MP_USER_GUARD_LOW, MP_USER_STACK, MP_USER_GUARD_HIGH,
    MP_HEAP, MP_REGION_COUNT
};
struct mp_span { uint32_t start, end; }; /* end exclusive */
struct mp_layout {
    struct mp_span regions[MP_REGION_COUNT];
    uint32_t heap_available;
};
enum mp_policy_mode { MP_LEGACY_ACTIVE, MP_PROTECTED_PLAN };

int mp_layout_validate(const struct mp_layout *layout, const char **why);
const char *mp_region_name(uint32_t id);
/* No NX bit exists in this non-PAE model. The plan is never installed in
   Phase A; ram_usable is a caller-supplied whole-page RAM classification. */
uint32_t mp_page_flags(const struct mp_layout *layout, uint32_t address,
                       enum mp_policy_mode mode, int ram_usable);
int mp_region_at(const struct mp_layout *layout, uint32_t address);
#endif
