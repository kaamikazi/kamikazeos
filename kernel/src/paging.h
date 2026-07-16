/*
 * paging.h - v2.3 paging structures and gated virtual memory foundation.
 *
 * Paging is enabled only after the kernel-owned GDT, early exception IDT,
 * page structures, and every required identity-mapped range validate.
 */
#ifndef KMZ_PAGING_H
#define KMZ_PAGING_H

#include <stdint.h>

#define PAGING_PAGE_SIZE 4096u
#define PAGING_PRESENT   0x001u
#define PAGING_WRITABLE  0x002u
#define PAGING_USER      0x004u
#define PAGING_PWT       0x008u
#define PAGING_PCD       0x010u
#define PAGING_MAX_RANGES 8u

struct paging_range {
    const char *name;
    uint32_t start;
    uint32_t end;
    int required;
    int validated;
};

struct paging_status {
    int prepared;
    int enabled;
    int identity_map_valid;
    int required_ranges_valid;
    int prerequisites_valid;
    uint32_t page_directory;
    uint32_t active_cr3;
    uint32_t mapped_start;
    uint32_t mapped_end;
    const char *mode;
    const char *page_fault_handler;
    const char *validation_error;
    uint32_t last_fault_address;
    uint32_t last_fault_error;
    uint32_t last_fault_eip;
    int last_fault_present;
};

void paging_init_structures(void);
int paging_validate_identity_map(void);
int paging_enable(void);
const struct paging_status *paging_status(void);
int paging_is_enabled(void);
uint32_t paging_range_count(void);
int paging_range_get(uint32_t index, struct paging_range *out);
void paging_record_page_fault(uint32_t address, uint32_t error_code, uint32_t eip);
void paging_trigger_pagefault_test(void);

#endif /* KMZ_PAGING_H */
