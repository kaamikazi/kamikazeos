/* paging.c - identity paging structures, validation, and fault evidence. */
#include "paging.h"
#include "errors.h"
#include "string.h"

#define PAGE_ENTRIES 1024u
#define IDENTITY_TABLES 4u
#define IDENTITY_BYTES (IDENTITY_TABLES * PAGE_ENTRIES * PAGING_PAGE_SIZE)

static uint32_t page_directory[PAGE_ENTRIES] __attribute__((aligned(PAGING_PAGE_SIZE)));
static uint32_t identity_tables[IDENTITY_TABLES][PAGE_ENTRIES]
    __attribute__((aligned(PAGING_PAGE_SIZE)));
static struct paging_status status;

static uint32_t read_cr0(void)
{
    uint32_t value;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(value));
    return value;
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
    status.identity_map_valid = paging_validate_identity_map();

    error_log(SEV_INFO, "PAGE-000", "Paging",
              "Identity paging structures prepared", "run paging for status");
}

int paging_validate_identity_map(void)
{
    if (!status.prepared)
        return 0;

    for (uint32_t table = 0; table < IDENTITY_TABLES; table++) {
        uint32_t expected_table = (uint32_t)&identity_tables[table][0];
        if ((page_directory[table] & PAGING_PRESENT) == 0)
            return 0;
        if ((page_directory[table] & 0xFFFFF000u) != expected_table)
            return 0;

        for (uint32_t i = 0; i < PAGE_ENTRIES; i++) {
            uint32_t phys = (table * PAGE_ENTRIES + i) * PAGING_PAGE_SIZE;
            uint32_t entry = identity_tables[table][i];
            if ((entry & PAGING_PRESENT) == 0)
                return 0;
            if ((entry & 0xFFFFF000u) != phys)
                return 0;
        }
    }
    return 1;
}

const struct paging_status *paging_status(void)
{
    status.enabled = paging_is_enabled();
    status.identity_map_valid = paging_validate_identity_map();
    status.mode = status.enabled ? "identity_enabled" : "prepared_not_enabled";
    return &status;
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
