#ifndef KMZ_EMERGENCY_RECORD_H
#define KMZ_EMERGENCY_RECORD_H
#include <stdint.h>
#include "emergency_abi.h"

#define EM_OUTPUT_LIMIT 1024u
#define EM_STACK_BYTES 16384u
#define EM_MIN_HEADROOM 1024u
enum em_stack_kind { EM_STACK_UNKNOWN, EM_STACK_BOOT, EM_STACK_PRIV, EM_STACK_FATAL, EM_STACK_DF };
enum em_unsafe {
    EM_BAD_FRAME=1u, EM_BAD_ORIGIN=2u, EM_BAD_STACK=4u, EM_NOT_READY=8u,
    EM_BAD_STORAGE=16u, EM_IO_ACTIVE=32u, EM_BAD_CONTEXT=64u,
    EM_GUARD_FAULT=128u, EM_PAGING_FAULT=256u, EM_BAD_CPU_STATE=512u
};
struct em_evidence {
    uint32_t kind, vector, error_code, cr2, eip, cs, eflags, cr0, cr3;
    uint32_t cpu_esp, origin_esp, diagnostic_esp, user_esp, user_ss;
    uint32_t stack_kind, stack_start, stack_end, frame_valid, origin_valid, eflags_known;
    uint32_t guard_hit, paging_structure_hit;
};
struct em_persistence_context {
    uint32_t runtime_ready, storage_ready, io_active, expected_cr3, text_start, text_end;
};
struct em_crash_record {
    uint32_t magic, version, kind, unsafe_flags, vector, error_code, cr2, eip, cs, eflags;
    uint32_t cr0, cr3, cpu_esp, origin_esp, diagnostic_esp, user_esp, user_ss;
    uint32_t stack_kind, stack_start, stack_end, frame_valid, origin_valid;
    uint32_t persistence_eligible, eflags_known, cr2_applicable, reserved[7];
};
typedef char em_record_size_check[sizeof(struct em_crash_record)==EM_RECORD_BYTES?1:-1];
typedef char em_record_vector_check[__builtin_offsetof(struct em_crash_record,vector)==ER_VECTOR?1:-1];
typedef char em_record_cr3_check[__builtin_offsetof(struct em_crash_record,cr3)==ER_CR3?1:-1];

uint32_t em_persistence_unsafe(const struct em_evidence *, const struct em_persistence_context *);
/* First-writer evidence is never reset or overwritten by this function. */
int em_record_capture(struct em_crash_record *, const struct em_evidence *, const struct em_persistence_context *);
typedef int (*em_putc_fn)(void *context, char value); /* must itself be bounded */
uint32_t em_record_emit(const struct em_crash_record *, em_putc_fn, void *context);
#endif
