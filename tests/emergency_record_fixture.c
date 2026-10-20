#include "../kernel/src/emergency_record.h"
#include "../kernel/src/string.h"

static struct em_evidence healthy(void)
{
    struct em_evidence e={0};e.kind=EM_EXCEPTION;e.vector=14;e.cr2=0xFFFFF000u;
    e.eip=0x100100;e.cs=8;e.eflags=0x202;e.cr0=0x80000011;e.cr3=0x140000;
    e.cpu_esp=0x152000;e.origin_esp=0x152010;e.diagnostic_esp=0x160000;
    e.stack_kind=EM_STACK_BOOT;e.stack_start=0x150000;e.stack_end=0x154000;
    e.frame_valid=e.origin_valid=e.eflags_known=1;return e;
}
static struct em_persistence_context context(void)
{ struct em_persistence_context c={1,1,0,0x140000,0x100000,0x120000};return c; }
static int equal(const void *a,const void *b,uint32_t n)
{ const uint8_t*x=a,*y=b;for(uint32_t i=0;i<n;i++)if(x[i]!=y[i])return 0;return 1; }
int test_verified_ordinary_faults_and_panics_are_eligible(void)
{
    struct em_evidence e=healthy();struct em_persistence_context c=context();
    for(uint32_t i=0;i<3;i++){e.vector=i==0?0:i==1?6:14;if(em_persistence_unsafe(&e,&c))return 0;}
    e.kind=EM_PANIC;e.vector=0xFFFFFFFFu;e.eflags_known=0;
    return !em_persistence_unsafe(&e,&c);
}
int test_emergency_reentry_and_df_never_persist(void)
{
    struct em_persistence_context c=context();
    for(uint32_t kind=EM_REENTRANT;kind<=EM_DOUBLE_FAULT;kind++) {
        struct em_evidence e=healthy();e.kind=kind;
        if(!(em_persistence_unsafe(&e,&c)&EM_BAD_CONTEXT))return 0;
    }
    struct em_evidence e=healthy();e.stack_kind=EM_STACK_FATAL;
    return (em_persistence_unsafe(&e,&c)&EM_BAD_STACK)!=0;
}
int test_io_unready_or_untrusted_storage_refuses_persistence(void)
{
    struct em_evidence e=healthy();struct em_persistence_context c=context();
    c.io_active=1;if(!(em_persistence_unsafe(&e,&c)&EM_IO_ACTIVE))return 0;
    c=context();c.storage_ready=0;if(!(em_persistence_unsafe(&e,&c)&EM_BAD_STORAGE))return 0;
    c=context();c.runtime_ready=0;return (em_persistence_unsafe(&e,&c)&EM_NOT_READY)!=0;
}
int test_bad_or_exhausted_stack_bounds_are_unsafe(void)
{
    struct em_persistence_context c=context();struct em_evidence e=healthy();
    e.cpu_esp=e.stack_start+512;if(!(em_persistence_unsafe(&e,&c)&EM_BAD_STACK))return 0;
    e=healthy();e.stack_end=e.stack_start;if(!(em_persistence_unsafe(&e,&c)&EM_BAD_STACK))return 0;
    e=healthy();e.origin_esp=e.stack_end+4;return (em_persistence_unsafe(&e,&c)&EM_BAD_STACK)!=0;
}
int test_unvalidated_origin_and_cpu_state_are_unsafe(void)
{
    struct em_evidence e=healthy();struct em_persistence_context c=context();
    e.frame_valid=0;if(!(em_persistence_unsafe(&e,&c)&EM_BAD_FRAME))return 0;
    e=healthy();e.cs=0x1B;if(!(em_persistence_unsafe(&e,&c)&EM_BAD_ORIGIN))return 0;
    e=healthy();e.cr3+=4096;if(!(em_persistence_unsafe(&e,&c)&EM_BAD_CPU_STATE))return 0;
    e=healthy();e.cr0=0;return (em_persistence_unsafe(&e,&c)&EM_BAD_CPU_STATE)!=0;
}
int test_guard_and_paging_faults_never_persist(void)
{
    struct em_evidence e=healthy();struct em_persistence_context c=context();
    e.guard_hit=1;if(!(em_persistence_unsafe(&e,&c)&EM_GUARD_FAULT))return 0;
    e=healthy();e.paging_structure_hit=1;if(!(em_persistence_unsafe(&e,&c)&EM_PAGING_FAULT))return 0;
    e=healthy();e.error_code=8;return (em_persistence_unsafe(&e,&c)&EM_PAGING_FAULT)!=0;
}
int test_fixed_record_captures_once_and_preserves_first_evidence(void)
{
    struct em_evidence e=healthy();struct em_persistence_context c=context();
    struct em_crash_record r={0};if(!em_record_capture(&r,&e,&c))return 0;
    struct em_crash_record saved=r;e.vector=6;
    return sizeof(r)==128 && r.magic==EM_MAGIC && r.cr2==e.cr2 && r.persistence_eligible &&
        !em_record_capture(&r,&e,&c) && equal(&r,&saved,sizeof(r));
}
static char output[EM_OUTPUT_LIMIT+1];static uint32_t sent,calls,fail_after;
static int put(void *unused,char c)
{ (void)unused;calls++;if(sent>=fail_after)return 0;output[sent++]=c;return 1; }
int test_formatter_has_fixed_output_and_stops_after_sink_failure(void)
{
    struct em_crash_record r={0};struct em_evidence e=healthy();struct em_persistence_context c=context();
    if(!em_record_capture(&r,&e,&c))return 0;
    sent=calls=0;fail_after=EM_OUTPUT_LIMIT;
    uint32_t n=em_record_emit(&r,put,0);
    if(n!=sent || n<100 || n>=EM_OUTPUT_LIMIT || output[0]!='[')return 0;
    sent=calls=0;fail_after=7;n=em_record_emit(&r,put,0);
    return n==7 && sent==7 && calls==8 && em_record_emit(&r,0,0)==0;
}
