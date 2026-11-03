#include <stdint.h>
#include "gdt.h"
#include "x86_task.h"

#define GDT_ENTRIES 7
#define GDT_USER_CODE_INDEX 3u
#define GDT_USER_DATA_INDEX 4u
#define GDT_TSS_INDEX       5u
#define GDT_DF_TSS_INDEX    6u

#define GDT_ACCESS_ACCESSED  0x01u
#define GDT_ACCESS_USER_CODE 0xFAu
#define GDT_ACCESS_USER_DATA 0xF2u

struct gdt_pointer {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

extern void gdt_load(const struct gdt_pointer *pointer);

static struct gdt_entry gdt[GDT_ENTRIES];
static struct gdt_pointer gdt_ptr;
static int loaded;
static int accessed_bit_validation;
static uint32_t tss_base;
static uint32_t tss_limit;
static uint32_t df_tss_base,df_tss_limit;
typedef char normal_tss_selector_unchanged[(GDT_TSS_INDEX*8u==0x28u)?1:-1];
typedef char df_tss_selector_check[(GDT_DF_TSS_INDEX*8u==GDT_DF_TSS_SELECTOR)?1:-1];

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

static int flat_segment_semantics_valid(const struct gdt_entry *entry,
                                        uint8_t expected_access)
{
    uint32_t base = (uint32_t)entry->base_low |
                    ((uint32_t)entry->base_middle << 16) |
                    ((uint32_t)entry->base_high << 24);
    uint32_t limit = (uint32_t)entry->limit_low |
                     ((uint32_t)(entry->granularity & 0x0Fu) << 16);
    uint8_t semantic_access =
        (uint8_t)(entry->access & (uint8_t)~GDT_ACCESS_ACCESSED);
    return base == 0 && limit == 0xFFFFFu &&
           semantic_access == expected_access &&
           (entry->granularity & 0xF0u) == 0xC0u;
}

static int gdt_accessed_bit_self_test(void)
{
    struct gdt_entry code = gdt[GDT_USER_CODE_INDEX];
    struct gdt_entry data = gdt[GDT_USER_DATA_INDEX];

    code.access &= (uint8_t)~GDT_ACCESS_ACCESSED;
    data.access &= (uint8_t)~GDT_ACCESS_ACCESSED;
    if (!flat_segment_semantics_valid(&code, GDT_ACCESS_USER_CODE) ||
        !flat_segment_semantics_valid(&data, GDT_ACCESS_USER_DATA))
        return 0;

    code.access |= GDT_ACCESS_ACCESSED;
    data.access |= GDT_ACCESS_ACCESSED;
    if (!flat_segment_semantics_valid(&code, GDT_ACCESS_USER_CODE) ||
        !flat_segment_semantics_valid(&data, GDT_ACCESS_USER_DATA))
        return 0;

    code.access ^= 0x20u; /* Changing DPL must never be masked as accessed. */
    data.granularity ^= 0x40u;
    return !flat_segment_semantics_valid(&code, GDT_ACCESS_USER_CODE) &&
           !flat_segment_semantics_valid(&data, GDT_ACCESS_USER_DATA);
}

void gdt_init(void)
{
    loaded = 0;
    accessed_bit_validation = 0;
    tss_base = 0;
    tss_limit = 0;
    df_tss_base=df_tss_limit=0;
    gdt_set_entry(0, 0, 0, 0, 0);
    gdt_set_entry(1, 0, 0xFFFFFu, 0x9Au, 0xC0u);
    gdt_set_entry(2, 0, 0xFFFFFu, 0x92u, 0xC0u);
    /* Set the CPU-managed accessed bit up front; validation also tolerates it. */
    gdt_set_entry(GDT_USER_CODE_INDEX, 0, 0xFFFFFu,
                  GDT_ACCESS_USER_CODE | GDT_ACCESS_ACCESSED, 0xC0u);
    gdt_set_entry(GDT_USER_DATA_INDEX, 0, 0xFFFFFu,
                  GDT_ACCESS_USER_DATA | GDT_ACCESS_ACCESSED, 0xC0u);
    gdt_set_entry(GDT_TSS_INDEX, 0, 0, 0, 0);
    gdt_set_entry(GDT_DF_TSS_INDEX, 0, 0, 0, 0);

    gdt_ptr.limit = (uint16_t)(sizeof(gdt) - 1u);
    gdt_ptr.base = (uint32_t)&gdt[0];
    gdt_load(&gdt_ptr);
    loaded = 1;
    accessed_bit_validation = gdt_accessed_bit_self_test();
}

int gdt_is_loaded(void)
{
    return loaded;
}

int gdt_user_segments_valid(void)
{
    return loaded && accessed_bit_validation &&
           flat_segment_semantics_valid(&gdt[GDT_USER_CODE_INDEX],
                                        GDT_ACCESS_USER_CODE) &&
           flat_segment_semantics_valid(&gdt[GDT_USER_DATA_INDEX],
                                        GDT_ACCESS_USER_DATA);
}

int gdt_install_tss_descriptor(uint32_t base, uint32_t limit)
{
    if (!loaded || !x86_make_tss_descriptor(&gdt[GDT_TSS_INDEX],base,limit))
        return 0;
    tss_base = base;
    tss_limit = limit;
    return gdt_tss_descriptor_valid();
}

int gdt_tss_descriptor_valid(void)
{
    return loaded && x86_tss_descriptor_valid(&gdt[GDT_TSS_INDEX],tss_base,tss_limit,1);
}

int gdt_install_df_tss_descriptor(uint32_t base,uint32_t limit)
{
    if(!loaded || !x86_make_tss_descriptor(&gdt[GDT_DF_TSS_INDEX],base,limit))return 0;
    df_tss_base=base;df_tss_limit=limit;return gdt_df_tss_descriptor_valid();
}
int gdt_df_tss_descriptor_valid(void)
{ return loaded && x86_tss_descriptor_valid(&gdt[GDT_DF_TSS_INDEX],df_tss_base,df_tss_limit,0); }

void gdt_status(struct gdt_status *out)
{
    if (!out)
        return;
    out->loaded = loaded;
    out->user_segments_valid = gdt_user_segments_valid();
    out->accessed_bit_validation = accessed_bit_validation;
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
