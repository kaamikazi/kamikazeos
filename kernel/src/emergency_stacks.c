#include "emergency_stacks.h"
#include "emergency_record.h"
#include "string.h"
static uint8_t fatal_stack[EM_STACK_BYTES]
    __attribute__((section(".fatal_stack"),aligned(4096)));
static uint8_t double_fault_stack[EM_STACK_BYTES]
    __attribute__((section(".df_stack"),aligned(4096)));
extern char _fatal_stack_start,_fatal_stack_end,_df_stack_start,_df_stack_end;
void emergency_stacks_init(void)
{ kmemset(fatal_stack,0,sizeof(fatal_stack));kmemset(double_fault_stack,0,sizeof(double_fault_stack)); }
uint32_t emergency_fatal_stack_bottom(void) { return (uint32_t)fatal_stack; }
uint32_t emergency_fatal_stack_top(void) { return (uint32_t)(fatal_stack+sizeof(fatal_stack)); }
uint32_t emergency_df_stack_bottom(void) { return (uint32_t)double_fault_stack; }
uint32_t emergency_df_stack_top(void) { return (uint32_t)(double_fault_stack+sizeof(double_fault_stack)); }
int emergency_stacks_valid(void)
{
    return emergency_fatal_stack_bottom()==(uint32_t)&_fatal_stack_start &&
        emergency_fatal_stack_top()==(uint32_t)&_fatal_stack_end &&
        emergency_df_stack_bottom()==(uint32_t)&_df_stack_start &&
        emergency_df_stack_top()==(uint32_t)&_df_stack_end &&
        ((emergency_fatal_stack_bottom()|emergency_fatal_stack_top()|
          emergency_df_stack_bottom()|emergency_df_stack_top())&4095u)==0;
}
