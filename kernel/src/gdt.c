#include <stdint.h>
#include "gdt.h"

#define GDT_ENTRIES 3

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
    gdt_set_entry(0, 0, 0, 0, 0);
    gdt_set_entry(1, 0, 0xFFFFFu, 0x9Au, 0xC0u);
    gdt_set_entry(2, 0, 0xFFFFFu, 0x92u, 0xC0u);

    gdt_ptr.limit = (uint16_t)(sizeof(gdt) - 1u);
    gdt_ptr.base = (uint32_t)&gdt[0];
    gdt_load(&gdt_ptr);
    loaded = 1;
}

int gdt_is_loaded(void)
{
    return loaded;
}
