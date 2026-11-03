#ifndef KMZ_EMERGENCY_STACKS_H
#define KMZ_EMERGENCY_STACKS_H
#include <stdint.h>
void emergency_stacks_init(void); /* boot only, never on the emergency stacks */
uint32_t emergency_fatal_stack_bottom(void);
uint32_t emergency_fatal_stack_top(void);
uint32_t emergency_df_stack_bottom(void);
uint32_t emergency_df_stack_top(void);
int emergency_stacks_valid(void);
#endif
