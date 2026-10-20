#include "emergency_record.h"
#include "memory_policy.h"
#include "gdt.h"
#include "string.h"

uint32_t em_persistence_unsafe(const struct em_evidence *e, const struct em_persistence_context *c)
{
    if (!e || !c) return EM_BAD_CONTEXT;
    uint32_t bad=0;
    if (e->frame_valid!=1) bad|=EM_BAD_FRAME;
    if (e->origin_valid!=1 || e->cs!=GDT_KERNEL_CODE_SELECTOR) bad|=EM_BAD_ORIGIN;
    if ((e->stack_kind!=EM_STACK_BOOT && e->stack_kind!=EM_STACK_PRIV) ||
        e->stack_start<0x100000u || e->stack_end>MP_APERTURE_END ||
        e->stack_end<=e->stack_start || e->stack_end-e->stack_start!=EM_STACK_BYTES ||
        ((e->stack_start|e->stack_end)&4095u) || e->cpu_esp<e->stack_start ||
        e->cpu_esp>e->stack_end || e->cpu_esp-e->stack_start<EM_MIN_HEADROOM ||
        e->origin_esp<e->cpu_esp || e->origin_esp>e->stack_end) bad|=EM_BAD_STACK;
    if (c->runtime_ready!=1) bad|=EM_NOT_READY;
    if (c->storage_ready!=1) bad|=EM_BAD_STORAGE;
    if (c->io_active) bad|=EM_IO_ACTIVE;
    if (e->kind!=EM_EXCEPTION && e->kind!=EM_PANIC) bad|=EM_BAD_CONTEXT;
    if (e->guard_hit) bad|=EM_GUARD_FAULT;
    if (e->paging_structure_hit || (e->vector==14 && (e->error_code&8u))) bad|=EM_PAGING_FAULT;
    if (!(e->cr0&0x80000000u) || !c->expected_cr3 || (c->expected_cr3&4095u) ||
        c->expected_cr3>=MP_APERTURE_END || (e->cr3&0xFFFFF000u)!=c->expected_cr3 ||
        c->text_start<0x100000u || c->text_end<=c->text_start || c->text_end>MP_APERTURE_END ||
        e->eip<c->text_start || e->eip>=c->text_end) bad|=EM_BAD_CPU_STATE;
    if (e->kind==EM_EXCEPTION) {
        if (e->vector>=32 || e->eflags_known!=1) bad|=EM_BAD_FRAME;
        switch (e->vector) {
        case 2: case 8: case 10: case 11: case 12: case 13: case 17: case 18:
            bad|=EM_BAD_CPU_STATE;break;
        }
    }
    return bad;
}

int em_record_capture(struct em_crash_record *r, const struct em_evidence *e,
                       const struct em_persistence_context *c)
{
    if (!r || !e || !c || r->magic) return 0;
    struct em_crash_record n;
    kmemset(&n,0,sizeof(n));n.version=EM_VERSION;n.kind=e->kind;
    n.unsafe_flags=em_persistence_unsafe(e,c);n.persistence_eligible=n.unsafe_flags==0;
    n.vector=e->vector;n.error_code=e->error_code;n.cr2=e->cr2;n.eip=e->eip;n.cs=e->cs;
    n.eflags=e->eflags;n.cr0=e->cr0;n.cr3=e->cr3;n.cpu_esp=e->cpu_esp;
    n.origin_esp=e->origin_esp;n.diagnostic_esp=e->diagnostic_esp;
    n.user_esp=e->user_esp;n.user_ss=e->user_ss;n.stack_kind=e->stack_kind;
    n.stack_start=e->stack_start;n.stack_end=e->stack_end;n.frame_valid=e->frame_valid;
    n.origin_valid=e->origin_valid;n.eflags_known=e->eflags_known;
    n.cr2_applicable=e->kind==EM_EXCEPTION && e->vector==14;
    memcpy(r,&n,sizeof(n));
    __atomic_signal_fence(__ATOMIC_RELEASE);
    r->magic=EM_MAGIC;
    return 1;
}

struct emitter { em_putc_fn put;void *context;uint32_t left,sent;int ok; };
static void emit_char(struct emitter *o, char c)
{
    if (!o->ok || !o->left) return;
    o->left--;
    if (!o->put(o->context,c)) o->ok=0;else o->sent++;
}
static void emit_text(struct emitter *o, const char *s)
{ for (uint32_t i=0;i<64u && s[i];i++) emit_char(o,s[i]); }
static void emit_hex(struct emitter *o, uint32_t value)
{
    static const char digits[]="0123456789ABCDEF";
    emit_text(o,"0x");
    for (int shift=28;shift>=0;shift-=4) emit_char(o,digits[(value>>shift)&15u]);
}
#define FIELD(name) emit_text(&o,#name "=");emit_hex(&o,r->name);emit_char(&o,'\n')
uint32_t em_record_emit(const struct em_crash_record *r, em_putc_fn put, void *context)
{
    if (!put) return 0;
    struct emitter o={put,context,EM_OUTPUT_LIMIT,0,1};
    emit_text(&o,"[EMERGENCY_CRASH]\n");
    if (!r || r->magic!=EM_MAGIC) {emit_text(&o,"record=incomplete\n");return o.sent;}
    FIELD(kind);FIELD(vector);FIELD(error_code);FIELD(cr2);FIELD(eip);FIELD(cs);FIELD(eflags);
    FIELD(cr0);FIELD(cr3);FIELD(cpu_esp);FIELD(origin_esp);FIELD(diagnostic_esp);
    FIELD(user_esp);FIELD(user_ss);FIELD(stack_kind);FIELD(stack_start);FIELD(stack_end);
    FIELD(frame_valid);FIELD(origin_valid);FIELD(eflags_known);FIELD(unsafe_flags);
    FIELD(persistence_eligible);
    emit_text(&o,"repair_attempted=no\n");
    return o.sent;
}
