#include "double_fault.h"
#include "emergency_stacks.h"
#include "gdt.h"
#include "idt.h"
#include "memory.h"
#include "memory_layout.h"
#include "paging.h"
#include "tss.h"

static struct x86_tss32 df_task __attribute__((aligned(16)));
static int prepared;
static const char *reason="not_prepared";
extern char _page_directory_start,_kernel_text_start,_kernel_text_end,_kernel_start,_kernel_end;
extern void emergency_double_fault_entry(void);
static uint16_t task_register(void)
{ uint16_t value;__asm__ volatile("str %0":"=r"(value));return value; }
static uint32_t current_cr3(void)
{ uint32_t value;__asm__ volatile("mov %%cr3,%0":"=r"(value));return value; }
static int state_valid(void)
{
    return x86_df_task_valid(&df_task,(uint32_t)&_page_directory_start,(uint32_t)&emergency_double_fault_entry,
        emergency_df_stack_bottom(),emergency_df_stack_top(),
        (uint32_t)&_kernel_text_start,(uint32_t)&_kernel_text_end);
}
int double_fault_init(void)
{
    if(prepared)return state_valid() && idt_double_fault_gate_valid();
    reason="architectural_prerequisites_missing";
    if(!gdt_is_loaded() || !idt_is_loaded() || !tss_is_loaded() || !tss_kernel_stack_valid() ||
       !paging_is_enabled() || !memory_layout_valid() || !emergency_stacks_valid())return 0;
    reason="legacy_mapping_or_runtime_ram_unverified";
    if(!paging_validate_identity_map() || (current_cr3()&0xFFFFF000u)!=(uint32_t)&_page_directory_start ||
       !memory_discovery_range_usable((uint32_t)&_kernel_start,(uint32_t)&_kernel_end))return 0;
    reason="df_task_or_descriptor_invalid";
    if(!x86_prepare_df_task(&df_task,(uint32_t)&_page_directory_start,(uint32_t)&emergency_double_fault_entry,
        emergency_df_stack_bottom(),emergency_df_stack_top(),
        (uint32_t)&_kernel_text_start,(uint32_t)&_kernel_text_end) || !state_valid() ||
       !gdt_install_df_tss_descriptor((uint32_t)&df_task,sizeof(df_task)-1u))return 0;
    reason="df_gate_invalid";
    if(!idt_install_double_fault_gate() || task_register()!=GDT_TSS_SELECTOR)return 0;
    prepared=1;reason="prepared_fault_execution_unverified";return 1;
}
void double_fault_status(struct double_fault_status *out)
{
    if(!out)return;
    out->prepared=prepared;out->descriptor_valid=gdt_df_tss_descriptor_valid();
    out->gate_valid=idt_double_fault_gate_valid();out->state_valid=prepared && state_valid();
    out->normal_selector=task_register();out->selector=GDT_DF_TSS_SELECTOR;
    out->tss_address=(uint32_t)&df_task;out->cr3=df_task.cr3;out->entry=df_task.eip;
    out->stack_start=emergency_df_stack_bottom();out->stack_end=emergency_df_stack_top();out->reason=reason;
}
const volatile struct x86_tss32 *double_fault_previous_state(void)
{
    /* Only hardware backlink evidence may identify the previous task. No claim
       is made that this is an original interrupt frame. */
    const volatile struct x86_tss32 *hardware=&df_task;
    if(task_register()!=GDT_DF_TSS_SELECTOR || (hardware->previous&0xFFFFu)!=GDT_TSS_SELECTOR)return 0;
    return tss_normal_state();
}
