#include "memory_policy_fixture.c"
#include "../kernel/src/emergency_frame.h"
#include "../kernel/src/x86_task.h"
#include "../kernel/src/gdt.h"
#include "../kernel/src/string.h"
static int prepare(struct x86_tss32 *t)
{ return x86_prepare_df_task(t,0x140000,0x100100,0x180000,0x184000,0x100000,0x120000); }
static int valid(const struct x86_tss32 *t)
{ return x86_df_task_valid(t,0x140000,0x100100,0x180000,0x184000,0x100000,0x120000); }
int test_tss_descriptors_encode_fixed_supervisor_state(void)
{
    struct gdt_entry d;
    return x86_make_tss_descriptor(&d,0x150000,103) && d.limit_low==103 && d.base_low==0 &&
        d.base_middle==0x15 && !d.base_high && d.access==0x89 && !d.granularity &&
        x86_tss_descriptor_valid(&d,0x150000,103,0) && GDT_TSS_SELECTOR==0x28 && GDT_DF_TSS_SELECTOR==0x30;
}
int test_descriptor_mutations_and_busy_policy_are_checked(void)
{
    struct gdt_entry d;if(!x86_make_tss_descriptor(&d,0x150000,103))return 0;
    for(uint32_t i=0;i<sizeof(d);i++) {
        struct gdt_entry n=d;((uint8_t*)&n)[i]^=0x40;
        if(x86_tss_descriptor_valid(&n,0x150000,103,0))return 0;
    }
    d.access=0x8B;
    return x86_tss_descriptor_valid(&d,0x150000,103,1) && !x86_tss_descriptor_valid(&d,0x150000,103,0);
}
int test_df_task_gate_is_exact_dpl0_selector_only(void)
{
    struct idt_entry g;
    if(x86_make_df_gate(&g,0x1B) || !x86_make_df_gate(&g,0x30) || !x86_df_gate_valid(&g))return 0;
    for(uint32_t i=0;i<sizeof(g);i++) {
        struct idt_entry n=g;((uint8_t*)&n)[i]^=1;
        if(x86_df_gate_valid(&n))return 0;
    }
    return g.flags==0x85 && !g.offset_low && !g.offset_high && !g.zero;
}
int test_df_task_state_has_valid_cr3_stack_and_integer_entry(void)
{
    struct x86_tss32 t;
    return prepare(&t) && valid(&t) && sizeof(t)==104 && t.cr3==0x140000 &&
        t.eip==0x100100 && t.esp==0x184000 && t.esp0==t.esp && t.cs==8 &&
        t.ss==16 && t.ss0==16 && t.ds==16 && t.es==16 && t.fs==16 && t.gs==16 &&
        t.eflags==2 && t.io_map_base==104 && !t.previous && !t.ldt && !t.trap;
}
int test_df_task_state_every_byte_is_structurally_checked(void)
{
    struct x86_tss32 t;if(!prepare(&t))return 0;
    for(uint32_t i=0;i<sizeof(t);i++) {
        struct x86_tss32 n=t;((uint8_t*)&n)[i]^=1;if(valid(&n))return 0;
    }
    return 1;
}
int test_invalid_task_parameters_are_refused(void)
{
    struct x86_tss32 t;struct gdt_entry d;
    if(x86_make_tss_descriptor(&d,0,103) || x86_make_tss_descriptor(&d,0x150001,103) ||
       x86_make_tss_descriptor(&d,0x150000,104))return 0;
    return !x86_prepare_df_task(&t,1,0x100100,0x180000,0x184000,0x100000,0x120000) &&
        !x86_prepare_df_task(&t,0x140000,0x120000,0x180000,0x184000,0x100000,0x120000) &&
        !x86_prepare_df_task(&t,0x140000,0x100100,0x140000,0x144000,0x100000,0x120000) &&
        !x86_prepare_df_task(&t,0x140000,0x100100,0x180000,0x184001,0x100000,0x120000);
}
static struct em_entry_state entry(const struct mp_layout *l,uint32_t id,uint32_t vector,uint32_t error)
{
    struct em_entry_state s={0};s.kind=EM_EXCEPTION;s.frame.vector=vector;s.has_error=error;
    s.cpu_esp=l->regions[id].start+2048;s.cr0=0x80000011;s.cr3=l->regions[MP_DIRECTORY].start;s.cr2=0xFFFFF000;
    return s;
}
int test_kernel_frame_without_error_preserves_accepted_abi(void)
{
    struct mp_layout l=layout();struct em_entry_state s=entry(&l,MP_BOOT_STACK,6,0);struct em_evidence e;
    const uint32_t words[]={0x100100,8,0x202,0xDEADBEEF,0xBADF00D};
    return em_decode_exception(&l,&s,words,&e) && e.frame_valid && e.origin_valid &&
        e.eip==words[0] && e.eflags==0x202 && !e.error_code &&
        s.frame.esp==s.cpu_esp-8 && e.origin_esp==s.cpu_esp+12 &&
        sizeof(struct interrupt_frame)==68 && __builtin_offsetof(struct em_entry_state,user)==68;
}
int test_kernel_error_frame_captures_early_cr2_and_exact_code(void)
{
    struct mp_layout l=layout();struct em_entry_state s=entry(&l,MP_BOOT_STACK,14,1);struct em_evidence e;
    const uint32_t words[]={3,0x100100,8,0x246};
    return em_decode_exception(&l,&s,words,&e) && e.error_code==3 && e.cr2==0xFFFFF000 &&
        e.cs==8 && e.eflags==0x246 && s.frame.esp==s.cpu_esp-4 && e.origin_esp==s.cpu_esp+16;
}
int test_user_tail_requires_privilege_stack_and_valid_selectors(void)
{
    struct mp_layout l=layout();struct em_entry_state s=entry(&l,MP_PRIV_STACK,14,1);struct em_evidence e;
    uint32_t words[]={5,0x100100,0x1B,0x202,l.regions[MP_USER_STACK].end,0x23};
    if(!em_decode_exception(&l,&s,words,&e) || !e.origin_valid || e.user_ss!=0x23 ||
       e.user_esp!=l.regions[MP_USER_STACK].end)return 0;
    s=entry(&l,MP_BOOT_STACK,14,1);
    return em_decode_exception(&l,&s,words,&e) && !e.origin_valid;
}
int test_short_bad_or_unclassified_frames_are_not_followed(void)
{
    struct mp_layout l=layout();struct em_entry_state s=entry(&l,MP_BOOT_STACK,14,1);struct em_evidence e;
    const uint32_t words[]={0,0x100100,8,0x202};
    s.cpu_esp=l.regions[MP_BOOT_STACK].end-4;
    if(em_decode_exception(&l,&s,words,&e) || e.frame_valid)return 0;
    s=entry(&l,MP_BOOT_STACK,14,1);s.cpu_esp=l.regions[MP_BOOT_GUARD_LOW].start;
    if(em_decode_exception(&l,&s,words,&e))return 0;
    s=entry(&l,MP_USER_STACK,14,1);
    return !em_decode_exception(&l,&s,words,&e) && !em_decode_exception(&l,&s,0,&e);
}
int test_same_cpl_never_uses_poisoned_user_tail(void)
{
    struct mp_layout l=layout();struct em_entry_state s=entry(&l,MP_PRIV_STACK,0,0);struct em_evidence e;
    const uint32_t words[]={0x100100,8,0x202,0xFFFFFFFF,0xFFFFFFFF};
    s.user.esp=s.user.ss=0xAAAAAAAA;
    return em_decode_exception(&l,&s,words,&e) && !s.user.esp && !s.user.ss && !e.user_esp && !e.user_ss;
}
int test_error_shape_and_conditional_tail_bounds_are_checked(void)
{
    struct mp_layout l=layout();struct em_entry_state s=entry(&l,MP_PRIV_STACK,14,0);struct em_evidence e;
    const uint32_t words[]={0,0x100100,0x1B,0x202,l.regions[MP_USER_STACK].end,0x23};
    if(em_decode_exception(&l,&s,words,&e))return 0;
    s=entry(&l,MP_PRIV_STACK,14,1);s.cpu_esp=l.regions[MP_PRIV_STACK].end-16;
    return !em_decode_exception(&l,&s,words,&e) && !e.frame_valid;
}
