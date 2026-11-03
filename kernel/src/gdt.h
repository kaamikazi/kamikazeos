#ifndef KMZ_GDT_H
#define KMZ_GDT_H

#include <stdint.h>

#define GDT_NULL_SELECTOR        0x00u
#define GDT_KERNEL_CODE_SELECTOR 0x08u
#define GDT_KERNEL_DATA_SELECTOR 0x10u
#define GDT_USER_CODE_SELECTOR   0x1Bu
#define GDT_USER_DATA_SELECTOR   0x23u
#define GDT_TSS_SELECTOR         0x28u

#define GDT_KERNEL_DPL 0u
#define GDT_USER_DPL   3u

struct gdt_status {
    int loaded;
    int user_segments_valid;
    int accessed_bit_validation;
    int tss_descriptor_valid;
    uint16_t kernel_code_selector;
    uint16_t kernel_data_selector;
    uint16_t user_code_selector;
    uint16_t user_data_selector;
    uint16_t tss_selector;
    uint8_t kernel_code_dpl;
    uint8_t kernel_data_dpl;
    uint8_t user_code_dpl;
    uint8_t user_data_dpl;
};

void gdt_init(void);
int gdt_is_loaded(void);
int gdt_user_segments_valid(void);
int gdt_install_tss_descriptor(uint32_t base, uint32_t limit);
int gdt_tss_descriptor_valid(void);
void gdt_status(struct gdt_status *out);

#endif /* KMZ_GDT_H */
