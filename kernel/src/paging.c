/* paging.c - identity paging structures, validation, and fault evidence. */
#include "paging.h"
#include "errors.h"
#include "gdt.h"
#include "idt.h"
#include "memory.h"
#include "string.h"

#define PAGE_ENTRIES 1024u
#define IDENTITY_TABLES 4u
#define IDENTITY_BYTES (IDENTITY_TABLES * PAGE_ENTRIES * PAGING_PAGE_SIZE)

static uint32_t page_directory[PAGE_ENTRIES] __attribute__((aligned(PAGING_PAGE_SIZE)));
static uint32_t identity_tables[IDENTITY_TABLES][PAGE_ENTRIES]
    __attribute__((aligned(PAGING_PAGE_SIZE)));
static struct paging_status status;
static struct paging_range ranges[PAGING_MAX_RANGES];
static uint32_t range_count;

extern char _kernel_start;
extern char _kernel_end;
extern char stack_bottom;
extern char stack_top;
extern void paging_enable_hardware(uint32_t page_directory_address);

static uint32_t read_cr0(void)
{
    uint32_t value;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(value));
    return value;
}

static uint32_t read_cr3(void)
{
    uint32_t value;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(value));
    return value;
}

static void add_range(const char *name, uint32_t start, uint32_t end,
                      int required)
{
    if (range_count >= PAGING_MAX_RANGES)
        return;
    ranges[range_count].name = name;
    ranges[range_count].start = start;
    ranges[range_count].end = end;
    ranges[range_count].required = required;
    ranges[range_count].validated = 0;
    range_count++;
}

static int identity_page_valid(uint32_t address)
{
    uint32_t page = address / PAGING_PAGE_SIZE;
    uint32_t table = page / PAGE_ENTRIES;
    uint32_t entry_index = page % PAGE_ENTRIES;
    if (table >= IDENTITY_TABLES)
        return 0;
    uint32_t pde = page_directory[table];
    uint32_t pte = identity_tables[table][entry_index];
    if ((pde & (PAGING_PRESENT | PAGING_WRITABLE)) !=
        (PAGING_PRESENT | PAGING_WRITABLE) || (pde & PAGING_USER))
        return 0;
    if ((pte & (PAGING_PRESENT | PAGING_WRITABLE)) !=
        (PAGING_PRESENT | PAGING_WRITABLE) || (pte & PAGING_USER))
        return 0;
    return (pte & 0xFFFFF000u) == (address & 0xFFFFF000u);
}

static int validate_range(struct paging_range *range)
{
    if (!range || range->end <= range->start || range->end > IDENTITY_BYTES)
        return 0;
    uint32_t page = range->start & 0xFFFFF000u;
    uint32_t last = (range->end - 1u) & 0xFFFFF000u;
    for (;;) {
        if (!identity_page_valid(page))
            return 0;
        if (page == last)
            break;
        if (page > 0xFFFFFFFFu - PAGING_PAGE_SIZE)
            return 0;
        page += PAGING_PAGE_SIZE;
    }
    return 1;
}

static void define_required_ranges(void)
{
    const struct memory_status *memory = memory_status();
    range_count = 0;
    add_range("low_memory_structures", 0, 0x00100000u, 1);
    add_range("vga_text", 0x000B8000u, 0x000B9000u, 1);
    add_range("kernel_image_and_static_buffers", (uint32_t)&_kernel_start,
              (uint32_t)&_kernel_end, 1);
    add_range("bootstrap_stack", (uint32_t)&stack_bottom,
              (uint32_t)&stack_top, 1);
    if (memory->heap_end > memory->heap_start)
        add_range("kernel_heap", memory->heap_start, memory->heap_end, 1);
    add_range("page_directory", (uint32_t)&page_directory[0],
              (uint32_t)&page_directory[0] + sizeof(page_directory), 1);
    add_range("page_tables", (uint32_t)&identity_tables[0][0],
              (uint32_t)&identity_tables[0][0] + sizeof(identity_tables), 1);
}

int paging_is_enabled(void)
{
    return (read_cr0() & 0x80000000u) != 0;
}

void paging_init_structures(void)
{
    kmemset(page_directory, 0, sizeof(page_directory));
    kmemset(identity_tables, 0, sizeof(identity_tables));
    kmemset(&status, 0, sizeof(status));
    kmemset(ranges, 0, sizeof(ranges));

    for (uint32_t table = 0; table < IDENTITY_TABLES; table++) {
        for (uint32_t i = 0; i < PAGE_ENTRIES; i++) {
            uint32_t phys = (table * PAGE_ENTRIES + i) * PAGING_PAGE_SIZE;
            identity_tables[table][i] = phys | PAGING_PRESENT | PAGING_WRITABLE;
        }
        page_directory[table] = ((uint32_t)&identity_tables[table][0]) |
                                PAGING_PRESENT | PAGING_WRITABLE;
    }

    status.prepared = 1;
    status.enabled = paging_is_enabled();
    status.page_directory = (uint32_t)&page_directory[0];
    status.mapped_start = 0;
    status.mapped_end = IDENTITY_BYTES;
    status.mode = status.enabled ? "identity_enabled" : "prepared_not_enabled";
    status.page_fault_handler = "diagnostic_halt";
    status.validation_error = "not_validated";
    define_required_ranges();
    status.identity_map_valid = paging_validate_identity_map();

    error_log(SEV_INFO, "PAGE-000", "Paging",
              "Identity paging structures prepared", "run paging for status");
}

int paging_validate_identity_map(void)
{
    status.required_ranges_valid = 0;
    status.prerequisites_valid = gdt_is_loaded() && idt_is_loaded();
    if (!status.prepared) {
        status.validation_error = "structures_not_prepared";
        return 0;
    }
    if (!status.prerequisites_valid) {
        status.validation_error = "gdt_or_exception_idt_missing";
        return 0;
    }
    if (((uint32_t)&page_directory[0] & (PAGING_PAGE_SIZE - 1u)) != 0) {
        status.validation_error = "page_directory_unaligned";
        return 0;
    }

    for (uint32_t table = 0; table < IDENTITY_TABLES; table++) {
        uint32_t expected_table = (uint32_t)&identity_tables[table][0];
        if ((expected_table & (PAGING_PAGE_SIZE - 1u)) != 0) {
            status.validation_error = "page_table_unaligned";
            return 0;
        }
        if ((page_directory[table] & PAGING_PRESENT) == 0)
            return 0;
        if ((page_directory[table] & PAGING_WRITABLE) == 0 ||
            (page_directory[table] & PAGING_USER) != 0) {
            status.validation_error = "page_directory_flags_invalid";
            return 0;
        }
        if ((page_directory[table] & 0xFFFFF000u) != expected_table)
            return 0;

        for (uint32_t i = 0; i < PAGE_ENTRIES; i++) {
            uint32_t phys = (table * PAGE_ENTRIES + i) * PAGING_PAGE_SIZE;
            uint32_t entry = identity_tables[table][i];
            if ((entry & PAGING_PRESENT) == 0)
                return 0;
            if ((entry & PAGING_WRITABLE) == 0 || (entry & PAGING_USER) != 0) {
                status.validation_error = "page_table_flags_invalid";
                return 0;
            }
            if ((entry & 0xFFFFF000u) != phys)
                return 0;
        }
    }
    for (uint32_t i = IDENTITY_TABLES; i < PAGE_ENTRIES; i++) {
        if (page_directory[i] != 0) {
            status.validation_error = "unexpected_page_directory_entry";
            return 0;
        }
    }
    for (uint32_t i = 0; i < range_count; i++) {
        ranges[i].validated = validate_range(&ranges[i]);
        if (ranges[i].required && !ranges[i].validated) {
            status.validation_error = "required_runtime_range_unmapped";
            return 0;
        }
    }
    status.required_ranges_valid = 1;
    status.validation_error = "none";
    return 1;
}

int paging_enable(void)
{
    status.identity_map_valid = paging_validate_identity_map();
    if (!status.identity_map_valid) {
        status.enabled = 0;
        status.mode = "prepared_not_enabled";
        error_log(SEV_WARNING, "PAGE-101", "Paging",
                  "Paging enable gate refused invalid identity map",
                  "kernel continues with paging disabled");
        return 0;
    }
    if (!paging_is_enabled())
        paging_enable_hardware((uint32_t)&page_directory[0]);
    status.active_cr3 = read_cr3();
    status.enabled = paging_is_enabled() &&
                     ((status.active_cr3 & 0xFFFFF000u) ==
                      ((uint32_t)&page_directory[0] & 0xFFFFF000u));
    status.mode = status.enabled ? "identity_enabled" : "prepared_not_enabled";
    if (!status.enabled) {
        status.validation_error = "hardware_enable_verification_failed";
        error_log(SEV_ERROR, "PAGE-102", "Paging",
                  "Paging hardware enable verification failed",
                  "halt testing and inspect CR0 and CR3");
        return 0;
    }
    error_log(SEV_INFO, "PAGE-001", "Paging",
              "Identity paging enabled after validation",
              "supervisor identity map covers required runtime ranges");
    return 1;
}

const struct paging_status *paging_status(void)
{
    status.enabled = paging_is_enabled();
    status.active_cr3 = status.enabled ? read_cr3() : 0;
    status.identity_map_valid = paging_validate_identity_map();
    status.mode = status.enabled ? "identity_enabled" : "prepared_not_enabled";
    return &status;
}

uint32_t paging_range_count(void)
{
    return range_count;
}

int paging_range_get(uint32_t index, struct paging_range *out)
{
    if (!out || index >= range_count)
        return 0;
    *out = ranges[index];
    return 1;
}

void paging_record_page_fault(uint32_t address, uint32_t error_code, uint32_t eip)
{
    status.last_fault_address = address;
    status.last_fault_error = error_code;
    status.last_fault_eip = eip;
    status.last_fault_present = 1;
}

void paging_trigger_pagefault_test(void)
{
    volatile uint32_t *bad = (volatile uint32_t *)0xFFFFF000u;
    volatile uint32_t value = *bad;
    (void)value;
}
