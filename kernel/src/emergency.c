#include "emergency.h"
#include "ata.h"
#include "double_fault.h"
#include "emergency_stacks.h"
#include "gdt.h"
#include "interrupts.h"
#include "memory_layout.h"
#include "recovery_memory.h"
#include "recovery_store.h"
#include "serial.h"
#include "string.h"

struct em_entry_state emergency_entry_state __attribute__((aligned(16)));
volatile uint32_t emergency_entry_busy,emergency_reentry_count;
struct em_crash_record emergency_first_record __attribute__((aligned(16)));
struct em_crash_record emergency_df_record __attribute__((aligned(16)));
struct em_crash_record emergency_reentry_record __attribute__((aligned(16)));
uint32_t emergency_df_cr2,emergency_df_cr0,emergency_df_cr3,emergency_df_error;
static int initialized,runtime_ready,ordinary_authorized;
extern char _page_directory_start;
extern void panic_ordinary(const char *,const char *,const char *) __attribute__((noreturn));

static uint32_t current_esp(void)
{ uint32_t value;__asm__ volatile("mov %%esp,%0":"=r"(value));return value; }
static int console_put(void *unused,char c)
{
    (void)unused;
    if(c=='\n' && !serial_putc_bounded('\r',4096u))return 0;
    return serial_putc_bounded(c,4096u);
}
void emergency_halt(void)
{ for(;;)__asm__ volatile("cli; hlt"); }
void emergency_init(void)
{
    if(initialized)return;
    kmemset(&emergency_entry_state,0,sizeof(emergency_entry_state));
    kmemset(&emergency_first_record,0,sizeof(emergency_first_record));
    kmemset(&emergency_df_record,0,sizeof(emergency_df_record));
    kmemset(&emergency_reentry_record,0,sizeof(emergency_reentry_record));
    emergency_entry_busy=emergency_reentry_count=0;runtime_ready=ordinary_authorized=0;
    emergency_stacks_init();memory_layout_init();initialized=1;
}
void emergency_set_runtime_ready(int ready) { runtime_ready=initialized && ready==1; }
int emergency_runtime_is_ready(void) { return runtime_ready; }
static int storage_ready(void)
{
    if(!ata_available() || ata_io_in_progress() || !recovery_store_valid())return 0;
    struct recovery_store_integrity i;recovery_store_integrity(&i);
    return !i.state_locked && !i.journal_locked && i.journal_scan_valid && i.journal_order_valid &&
        recovery_memory_get()->boot_order_trusted;
}
static struct em_persistence_context context(void)
{
    const struct mp_layout *l=memory_layout_get();
    struct em_persistence_context c={0};
    c.runtime_ready=runtime_ready;c.storage_ready=runtime_ready && storage_ready();
    c.io_active=ata_io_in_progress();c.expected_cr3=(uint32_t)&_page_directory_start;
    c.text_start=l->regions[MP_TEXT].start;c.text_end=l->regions[MP_TEXT].end;return c;
}
int emergency_persistence_allowed_now(void)
{
    return ordinary_authorized && (emergency_entry_busy&1u) && runtime_ready &&
        emergency_first_record.magic==EM_MAGIC && emergency_first_record.persistence_eligible==1 && storage_ready();
}
int emergency_exception_authorized(const struct interrupt_frame *f)
{
    return f==&emergency_entry_state.frame && emergency_first_record.kind==EM_EXCEPTION &&
        emergency_persistence_allowed_now();
}
int emergency_panic_authorized(void)
{ return emergency_first_record.kind==EM_PANIC && emergency_persistence_allowed_now(); }

static void memory_fault_classification(const struct mp_layout *l,struct em_evidence *e)
{
    if(e->kind!=EM_EXCEPTION || e->vector!=14)return;
    int id=mp_region_at(l,e->cr2);
    e->guard_hit=id>=0 && mp_region_is_guard((uint32_t)id);
    e->paging_structure_hit=id==MP_DIRECTORY || id==MP_TABLES;
}
static void copy_panic_text(char *out,uint32_t cap,const char *input)
{
    const struct mp_layout *l=memory_layout_get();uint32_t at=(uint32_t)input;
    int id=mp_region_at(l,at);uint32_t n=0;
    if(id>=MP_TEXT && id<MP_USER_CODE && !mp_region_is_guard((uint32_t)id) &&
       id!=MP_DIRECTORY && id!=MP_TABLES) {
        while(n+1<cap && at<l->regions[id].end) {char c=*(const char*)at++;if(!c)break;out[n++]=c;}
    }
    out[n]=0;
}
void emergency_handle_entry(void)
{
    const struct mp_layout *l=memory_layout_get();struct em_evidence e;
    uint32_t *words=(uint32_t*)emergency_entry_state.cpu_esp;
    uint32_t panic_args[3]={0};
    if(emergency_entry_state.kind==EM_EXCEPTION) {
        em_decode_exception(l,&emergency_entry_state,words,&e);
    } else if(emergency_entry_state.kind==EM_PANIC) {
        em_entry_evidence(&emergency_entry_state,0,&e);
        int stack=em_source_stack(l,e.cpu_esp,16,&e.stack_start,&e.stack_end);
        if(stack) {
            e.eip=words[0];e.origin_esp=e.cpu_esp+16;e.stack_kind=(uint32_t)stack;
            e.frame_valid=1;e.origin_valid=e.cs==GDT_KERNEL_CODE_SELECTOR;e.context_source=EM_SOURCE_CALL_FRAME;
            for(uint32_t i=0;i<3;i++)panic_args[i]=words[i+1];
        }
    } else em_entry_evidence(&emergency_entry_state,0,&e);
    e.diagnostic_esp=current_esp();memory_fault_classification(l,&e);
    struct em_persistence_context c=context();
    int captured=em_record_capture(&emergency_first_record,&e,&c);
    em_record_emit(&emergency_first_record,console_put,0);
    ordinary_authorized=captured && emergency_first_record.persistence_eligible==1;
    if(!ordinary_authorized)emergency_halt();
    if(e.kind==EM_EXCEPTION) {
        interrupt_dispatch(&emergency_entry_state.frame);
        emergency_halt(); /* exceptions never resume, even if a dispatcher regresses */
    }
    char code[32],message[96],hint[96];
    copy_panic_text(code,sizeof(code),(const char*)panic_args[0]);
    copy_panic_text(message,sizeof(message),(const char*)panic_args[1]);
    copy_panic_text(hint,sizeof(hint),(const char*)panic_args[2]);
    panic_ordinary(code,message,hint);
}
void emergency_double_fault_c(void)
{
    struct em_evidence e={0};struct em_persistence_context c={0};
    e.kind=EM_DOUBLE_FAULT;e.vector=8;e.error_code=emergency_df_error;
    e.cr2=emergency_df_cr2;e.cr0=emergency_df_cr0;e.cr3=emergency_df_cr3;
    e.stack_kind=EM_STACK_DF;e.stack_start=emergency_df_stack_bottom();
    e.stack_end=emergency_df_stack_top();e.diagnostic_esp=current_esp();
    const volatile struct x86_tss32 *previous=double_fault_previous_state();
    if(previous) {
        e.eip=previous->eip;e.cs=previous->cs;e.eflags=previous->eflags;
        e.cpu_esp=e.origin_esp=previous->esp;e.context_source=EM_SOURCE_PREVIOUS_TSS;e.eflags_known=1;
    }
    /* Never call normal logging, store/CMOS, allocator, mapping or task-return. */
    em_record_capture(&emergency_df_record,&e,&c);
    em_record_emit(&emergency_df_record,console_put,0);emergency_halt();
}
