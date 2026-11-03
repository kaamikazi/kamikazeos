#include "x86_task.h"
#include "gdt.h"
#include "memory_policy.h"
#include "emergency_record.h"
#include "string.h"

int x86_make_tss_descriptor(struct gdt_entry *d,uint32_t base,uint32_t limit)
{
    if(!d || !base || (base&15u) || limit!=sizeof(struct x86_tss32)-1u ||
       (uint64_t)base+limit>=MP_APERTURE_END)return 0;
    d->limit_low=(uint16_t)limit;d->base_low=(uint16_t)base;d->base_middle=(uint8_t)(base>>16);
    d->access=0x89;d->granularity=0;d->base_high=(uint8_t)(base>>24);return 1;
}
int x86_tss_descriptor_valid(const struct gdt_entry *d,uint32_t base,uint32_t limit,int allow_busy)
{
    struct gdt_entry expected;
    if(!d || !x86_make_tss_descriptor(&expected,base,limit))return 0;
    uint8_t access=d->access;
    if(allow_busy && access==0x8B)access=0x89;
    return d->limit_low==expected.limit_low && d->base_low==expected.base_low &&
        d->base_middle==expected.base_middle && access==expected.access &&
        d->granularity==expected.granularity && d->base_high==expected.base_high;
}
int x86_make_df_gate(struct idt_entry *gate,uint16_t selector)
{
    if(!gate || selector!=GDT_DF_TSS_SELECTOR)return 0;
    gate->offset_low=0;gate->selector=selector;gate->zero=0;gate->flags=0x85;gate->offset_high=0;return 1;
}
int x86_df_gate_valid(const struct idt_entry *g)
{ return g && !g->offset_low && g->selector==GDT_DF_TSS_SELECTOR && !g->zero && g->flags==0x85 && !g->offset_high; }

int x86_prepare_df_task(struct x86_tss32 *t,uint32_t cr3,uint32_t eip,uint32_t bottom,
                        uint32_t top,uint32_t code_start,uint32_t code_end)
{
    if(!t || !cr3 || (cr3&4095u) || cr3>MP_APERTURE_END-MP_PAGE_SIZE ||
       bottom<0x100000u || top<=bottom || top-bottom!=EM_STACK_BYTES || top>MP_APERTURE_END ||
       ((bottom|top|code_start|code_end)&4095u) || code_start<0x100000u || code_end<=code_start ||
       code_end>MP_APERTURE_END || eip<code_start || eip>=code_end ||
       (bottom<code_end && top>code_start) || (bottom<cr3+MP_PAGE_SIZE && top>cr3))return 0;
    kmemset(t,0,sizeof(*t));t->esp0=t->esp=top;t->ss0=t->ss=GDT_KERNEL_DATA_SELECTOR;
    t->cr3=cr3;t->eip=eip;t->eflags=2;t->cs=GDT_KERNEL_CODE_SELECTOR;
    t->ds=t->es=t->fs=t->gs=GDT_KERNEL_DATA_SELECTOR;t->io_map_base=sizeof(*t);return 1;
}
int x86_df_task_valid(const struct x86_tss32 *t,uint32_t cr3,uint32_t eip,uint32_t bottom,
                       uint32_t top,uint32_t code_start,uint32_t code_end)
{
    struct x86_tss32 expected;
    if(!t || !x86_prepare_df_task(&expected,cr3,eip,bottom,top,code_start,code_end))return 0;
    const uint8_t *a=(const uint8_t*)t,*b=(const uint8_t*)&expected;
    for(uint32_t i=0;i<sizeof(*t);i++)if(a[i]!=b[i])return 0;
    return 1;
}
