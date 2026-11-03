#ifndef KMZ_EMERGENCY_H
#define KMZ_EMERGENCY_H
#include "emergency_frame.h"
extern struct em_entry_state emergency_entry_state;
extern volatile uint32_t emergency_entry_busy,emergency_reentry_count;
extern struct em_crash_record emergency_first_record,emergency_df_record,emergency_reentry_record;
void emergency_init(void);
void emergency_set_runtime_ready(int ready);
int emergency_runtime_is_ready(void);
int emergency_persistence_allowed_now(void);
int emergency_exception_authorized(const struct interrupt_frame *);
int emergency_panic_authorized(void);
void emergency_handle_entry(void) __attribute__((noreturn));
void emergency_double_fault_c(void) __attribute__((noreturn));
void emergency_reentry_halt(void) __attribute__((noreturn));
void emergency_halt(void) __attribute__((noreturn));
#endif
