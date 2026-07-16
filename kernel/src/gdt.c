#include <stdint.h>
#include "gdt.h"

#define GDT_ENTRIES 6
#define GDT_USER_CODE_INDEX 3u
#define GDT_USER_DATA_INDEX 4u
#define GDT_TSS_INDEX       5u

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} __attribute__((packed));

struct gdt_pointer {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

extern void gdt_load(const struct gdt_pointer *pointer);

static struct gdt_entry gdt[GDT_ENTRIES];
static struct gdt_pointer gdt_ptr;
static int loaded;
static uint32_t tss_base;
static uint32_t tss_limit;

static void gdt_set_entry(uint32_t index, uint32_t base, uint32_t limit,
                          uint8_t access, uint8_t flags)
{
    gdt[index].limit_low = (uint16_t)(limit & 0xFFFFu);
    gdt[index].base_low = (uint16_t)(base & 0xFFFFu);
    gdt[index].base_middle = (uint8_t)((base >> 16) & 0xFFu);
    gdt[index].access = access;
    gdt[index].granularity = (uint8_t)(((limit >> 16) & 0x0Fu) |
                                       (flags & 0xF0u));
    gdt[index].base_high = (uint8_t)((base >> 24) & 0xFFu);
}

void gdt_init(void)
{
    loaded = 0;
    tss_base = 0;
    tss_limit = 0;
    gdt_set_entry(0, 0, 0, 0, 0);
    gdt_set_entry(1, 0, 0xFFFFFu, 0x9Au, 0xC0u);
    gdt_set_entry(2, 0, 0xFFFFFu, 0x92u, 0xC0u);
    gdt_set_entry(GDT_USER_CODE_INDEX, 0, 0xFFFFFu, 0xFAu, 0xC0u);
    gdt_set_entry(GDT_USER_DATA_INDEX, 0, 0xFFFFFu, 0xF2u, 0xC0u);
    gdt_set_entry(GDT_TSS_INDEX, 0, 0, 0, 0);

    gdt_ptr.limit = (uint16_t)(sizeof(gdt) - 1u);
    gdt_ptr.base = (uint32_t)&gdt[0];
    gdt_load(&gdt_ptr);
    loaded = 1;
}

int gdt_is_loaded(void)
{
    return loaded;
}

int gdt_user_segments_valid(void)
{
    return loaded &&
           gdt[GDT_USER_CODE_INDEX].base_low == 0 &&
           gdt[GDT_USER_CODE_INDEX].base_middle == 0 &&
           gdt[GDT_USER_CODE_INDEX].base_high == 0 &&
           gdt[GDT_USER_CODE_INDEX].limit_low == 0xFFFFu &&
           gdt[GDT_USER_CODE_INDEX].access == 0xFAu &&
           gdt[GDT_USER_CODE_INDEX].granularity == 0xCFu &&
           gdt[GDT_USER_DATA_INDEX].base_low == 0 &&
           gdt[GDT_USER_DATA_INDEX].base_middle == 0 &&
           gdt[GDT_USER_DATA_INDEX].base_high == 0 &&
           gdt[GDT_USER_DATA_INDEX].limit_low == 0xFFFFu &&
           gdt[GDT_USER_DATA_INDEX].access == 0xF2u &&
           gdt[GDT_USER_DATA_INDEX].granularity == 0xCFu;
}

int gdt_install_tss_descriptor(uint32_t base, uint32_t limit)
{
    if (!loaded || base == 0 || limit < 103u || limit > 0xFFFFFu)
        return 0;
    tss_base = base;
    tss_limit = limit;
    gdt_set_entry(GDT_TSS_INDEX, base, limit, 0x89u, 0x00u);
    return gdt_tss_descriptor_valid();
}

int gdt_tss_descriptor_valid(void)
{
    uint32_t base = (uint32_t)gdt[GDT_TSS_INDEX].base_low |
                    ((uint32_t)gdt[GDT_TSS_INDEX].base_middle << 16) |
                    ((uint32_t)gdt[GDT_TSS_INDEX].base_high << 24);
    uint32_t limit = (uint32_t)gdt[GDT_TSS_INDEX].limit_low |
                     ((uint32_t)(gdt[GDT_TSS_INDEX].granularity & 0x0Fu) << 16);
    uint8_t type = (uint8_t)(gdt[GDT_TSS_INDEX].access & 0x0Fu);
    return loaded && base == tss_base && limit == tss_limit &&
           (gdt[GDT_TSS_INDEX].access & 0x80u) != 0 &&
           (gdt[GDT_TSS_INDEX].access & 0x10u) == 0 &&
           (type == 0x09u || type == 0x0Bu) &&
           (gdt[GDT_TSS_INDEX].granularity & 0xF0u) == 0;
}

void gdt_status(struct gdt_status *out)
{
    if (!out)
        return;
    out->loaded = loaded;
    out->user_segments_valid = gdt_user_segments_valid();
    out->tss_descriptor_valid = gdt_tss_descriptor_valid();
    out->kernel_code_selector = GDT_KERNEL_CODE_SELECTOR;
    out->kernel_data_selector = GDT_KERNEL_DATA_SELECTOR;
    out->user_code_selector = GDT_USER_CODE_SELECTOR;
    out->user_data_selector = GDT_USER_DATA_SELECTOR;
    out->tss_selector = GDT_TSS_SELECTOR;
    out->kernel_code_dpl = GDT_KERNEL_DPL;
    out->kernel_data_dpl = GDT_KERNEL_DPL;
    out->user_code_dpl = GDT_USER_DPL;
    out->user_data_dpl = GDT_USER_DPL;
}
