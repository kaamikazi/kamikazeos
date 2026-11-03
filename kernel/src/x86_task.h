/* Pure, testable 32-bit descriptor/task-state encoding, no task scheduler. */
#ifndef KMZ_X86_TASK_H
#define KMZ_X86_TASK_H
#include <stdint.h>
struct gdt_entry {
    uint16_t limit_low,base_low;
    uint8_t base_middle,access,granularity,base_high;
} __attribute__((packed));
struct idt_entry {
    uint16_t offset_low,selector;
    uint8_t zero,flags;
    uint16_t offset_high;
} __attribute__((packed));
struct x86_tss32 {
    uint32_t previous,esp0,ss0,esp1,ss1,esp2,ss2,cr3,eip,eflags;
    uint32_t eax,ecx,edx,ebx,esp,ebp,esi,edi;
    uint32_t es,cs,ss,ds,fs,gs,ldt;
    uint16_t trap,io_map_base;
} __attribute__((packed));
typedef char x86_descriptor_size[sizeof(struct gdt_entry)==8?1:-1];
typedef char x86_gate_size[sizeof(struct idt_entry)==8?1:-1];
typedef char x86_tss_size[sizeof(struct x86_tss32)==104?1:-1];
typedef char x86_tss_cr3_offset[__builtin_offsetof(struct x86_tss32,cr3)==28?1:-1];
typedef char x86_tss_eip_offset[__builtin_offsetof(struct x86_tss32,eip)==32?1:-1];
typedef char x86_tss_esp_offset[__builtin_offsetof(struct x86_tss32,esp)==56?1:-1];
typedef char x86_tss_io_offset[__builtin_offsetof(struct x86_tss32,io_map_base)==102?1:-1];
int x86_make_tss_descriptor(struct gdt_entry *,uint32_t base,uint32_t limit);
int x86_tss_descriptor_valid(const struct gdt_entry *,uint32_t base,uint32_t limit,int allow_busy);
int x86_make_df_gate(struct idt_entry *,uint16_t selector);
int x86_df_gate_valid(const struct idt_entry *);
int x86_prepare_df_task(struct x86_tss32 *,uint32_t cr3,uint32_t eip,
                        uint32_t bottom,uint32_t top,uint32_t code_start,uint32_t code_end);
int x86_df_task_valid(const struct x86_tss32 *,uint32_t cr3,uint32_t eip,
                       uint32_t bottom,uint32_t top,uint32_t code_start,uint32_t code_end);
#endif
