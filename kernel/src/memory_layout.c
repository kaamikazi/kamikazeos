/* Bind pure policy to linker symbols; never writes PDEs/PTEs/CR0. */
#include "memory_layout.h"
#include "memory.h"
#include "string.h"

#define DECLARE(name) extern char name##_start, name##_end
DECLARE(_kernel_text); DECLARE(_kernel_rodata); DECLARE(_kernel_data); DECLARE(_kernel_bss);
DECLARE(_page_directory); DECLARE(_page_tables);
DECLARE(_bootstrap_guard_low); DECLARE(_bootstrap_stack); DECLARE(_bootstrap_guard_high);
DECLARE(_privilege_guard_low); DECLARE(_privilege_stack); DECLARE(_privilege_guard_high);
DECLARE(_fatal_guard_low); DECLARE(_fatal_stack); DECLARE(_fatal_guard_high);
DECLARE(_df_guard_low); DECLARE(_df_stack); DECLARE(_df_guard_high);
DECLARE(_user_code); DECLARE(_user_guard_low); DECLARE(_user_stack); DECLARE(_user_guard_high);
DECLARE(_heap_candidate);
static struct mp_layout layout;
static int valid;
static const char *error="not_initialized";
#define SPAN(name) { (uint32_t)&name##_start, (uint32_t)&name##_end }

void memory_layout_init(void)
{
    const struct mp_span linked[MP_REGION_COUNT] = {
        {0,MP_PAGE_SIZE},{MP_PAGE_SIZE,0xB8000u},{0xB8000u,0xB9000u},{0xB9000u,0x100000u},
        SPAN(_kernel_text),SPAN(_kernel_rodata),SPAN(_kernel_data),SPAN(_kernel_bss),
        SPAN(_page_directory),SPAN(_page_tables),
        SPAN(_bootstrap_guard_low),SPAN(_bootstrap_stack),SPAN(_bootstrap_guard_high),
        SPAN(_privilege_guard_low),SPAN(_privilege_stack),SPAN(_privilege_guard_high),
        SPAN(_fatal_guard_low),SPAN(_fatal_stack),SPAN(_fatal_guard_high),
        SPAN(_df_guard_low),SPAN(_df_stack),SPAN(_df_guard_high),
        SPAN(_user_code),SPAN(_user_guard_low),SPAN(_user_stack),SPAN(_user_guard_high),
        SPAN(_heap_candidate)
    };
    memcpy(layout.regions,linked,sizeof(linked));
    const struct memory_status *m=memory_status();
    layout.heap_available=m->heap_end>m->heap_start;
    valid=mp_layout_validate(&layout,&error);
    if (valid && layout.heap_available &&
        (m->heap_start!=layout.regions[MP_HEAP].start || m->heap_end!=layout.regions[MP_HEAP].end)) {
        valid=0;error="runtime_heap_differs_from_linked_candidate";
    }
}
const struct mp_layout *memory_layout_get(void) { return &layout; }
int memory_layout_valid(void) { return valid; }
const char *memory_layout_error(void) { return error; }
