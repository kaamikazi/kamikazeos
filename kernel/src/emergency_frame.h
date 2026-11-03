#ifndef KMZ_EMERGENCY_FRAME_H
#define KMZ_EMERGENCY_FRAME_H
#include "emergency_record.h"
#include "interrupts.h"
#include "memory_policy.h"
/* frame+user retain the accepted 68-byte prefix / conditional 8-byte tail. */
struct em_entry_state {
    struct interrupt_frame frame;
    struct interrupt_user_stack user;
    uint32_t cpu_esp,cr2,cr0,cr3,has_error,kind;
};
typedef char em_entry_size[sizeof(struct em_entry_state)==EM_ENTRY_BYTES?1:-1];
typedef char em_entry_user_tail[__builtin_offsetof(struct em_entry_state,user)==68?1:-1];
typedef char em_entry_esp[__builtin_offsetof(struct em_entry_state,cpu_esp)==EE_CPU_ESP?1:-1];
typedef char em_entry_cr3[__builtin_offsetof(struct em_entry_state,cr3)==EE_CR3?1:-1];
typedef char em_entry_kind[__builtin_offsetof(struct em_entry_state,kind)==EE_KIND?1:-1];
int em_source_stack(const struct mp_layout *,uint32_t address,uint32_t bytes,
                     uint32_t *start,uint32_t *end);
/* words is read only after the supplied architectural address is in a known
   stack. Runtime passes that exact address; host tests use an independent buffer. */
int em_decode_exception(const struct mp_layout *,struct em_entry_state *,
                         const uint32_t *words,struct em_evidence *);
void em_entry_evidence(const struct em_entry_state *,uint32_t diagnostic_esp,struct em_evidence *);
#endif
