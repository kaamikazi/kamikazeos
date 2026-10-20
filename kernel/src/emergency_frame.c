#include "emergency_frame.h"
#include "gdt.h"
#include "string.h"

int em_source_stack(const struct mp_layout *l,uint32_t address,uint32_t bytes,uint32_t *start,uint32_t *end)
{
    if(!mp_layout_validate(l,0) || !bytes || (address&3u))return EM_STACK_UNKNOWN;
    const uint32_t ids[]={MP_BOOT_STACK,MP_PRIV_STACK,MP_FATAL_STACK,MP_DF_STACK};
    for(uint32_t i=0;i<4;i++) {
        const struct mp_span *s=&l->regions[ids[i]];
        if(address>=s->start && bytes<=s->end-s->start && address<=s->end-bytes) {
            if(start)*start=s->start;if(end)*end=s->end;return (int)i+1;
        }
    }
    return EM_STACK_UNKNOWN;
}
void em_entry_evidence(const struct em_entry_state *s,uint32_t diagnostic_esp,struct em_evidence *e)
{
    kmemset(e,0,sizeof(*e));if(!s)return;
    e->kind=s->kind;e->vector=s->frame.vector;e->error_code=s->frame.error_code;e->eip=s->frame.eip;
    e->cs=s->frame.cs;e->eflags=s->frame.eflags;e->cr2=s->cr2;e->cr0=s->cr0;e->cr3=s->cr3;
    e->cpu_esp=s->cpu_esp;e->diagnostic_esp=diagnostic_esp;e->user_esp=s->user.esp;e->user_ss=s->user.ss;
}
static int has_error(uint32_t vector)
{
    switch(vector) {case 8:case 10:case 11:case 12:case 13:case 14:case 17:case 21:case 29:case 30:return 1;default:return 0;}
}
int em_decode_exception(const struct mp_layout *l,struct em_entry_state *s,
                         const uint32_t *words,struct em_evidence *e)
{
    if(!s || !e)return 0;
    em_entry_evidence(s,0,e);
    if(!words || s->kind!=EM_EXCEPTION || s->frame.vector>=32 ||
       s->has_error!=(uint32_t)has_error(s->frame.vector))return 0;
    uint32_t offset=s->has_error?1u:0u,bytes=(3u+offset)*4u;
    int stack=em_source_stack(l,s->cpu_esp,bytes,&e->stack_start,&e->stack_end);
    if(!stack)return 0;
    /* CPU has already delivered this bounded frame; no saved ESP/EIP is ever
       followed as a pointer. The optional tail requires a second bounds check. */
    uint32_t cs=words[offset+1];
    if((cs&3u)==3u) {
        bytes+=8;
        if(!em_source_stack(l,s->cpu_esp,bytes,0,0))return 0;
        s->user.esp=words[offset+3];s->user.ss=words[offset+4];
    } else {s->user.esp=0;s->user.ss=0;}
    s->frame.error_code=offset?words[0]:0;s->frame.eip=words[offset];s->frame.cs=cs;
    s->frame.eflags=words[offset+2];
    /* Preserve the accepted PUSHA-time ESP convention, not a fake pre-fault ESP. */
    s->frame.esp=s->cpu_esp-(offset?4u:8u);
    em_entry_evidence(s,0,e);e->stack_kind=(uint32_t)stack;
    em_source_stack(l,s->cpu_esp,bytes,&e->stack_start,&e->stack_end);
    e->origin_esp=s->cpu_esp+bytes;e->frame_valid=1;e->eflags_known=1;e->context_source=EM_SOURCE_CPU_FRAME;
    e->origin_valid=cs==GDT_KERNEL_CODE_SELECTOR ||
        (cs==GDT_USER_CODE_SELECTOR && stack==EM_STACK_PRIV && s->user.ss==GDT_USER_DATA_SELECTOR &&
         s->user.esp>l->regions[MP_USER_STACK].start && s->user.esp<=l->regions[MP_USER_STACK].end);
    return 1;
}
