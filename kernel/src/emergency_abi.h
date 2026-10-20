/* Shared numeric ABI for C and preprocessed assembly; RAM-only, not disk format. */
#ifndef KMZ_EMERGENCY_ABI_H
#define KMZ_EMERGENCY_ABI_H
#define EM_MAGIC 0x454D4352
#define EM_VERSION 1
#define EM_EXCEPTION 1
#define EM_REENTRANT 2
#define EM_DOUBLE_FAULT 3
#define EM_PANIC 4
#define EM_RECORD_BYTES 128
#define EM_ENTRY_BYTES 100
#define EE_VECTOR 48
#define EE_CPU_ESP 76
#define EE_CR2 80
#define EE_CR0 84
#define EE_CR3 88
#define EE_HAS_ERROR 92
#define EE_KIND 96
#define ER_KIND 8
#define ER_VECTOR 16
#define ER_CR2 24
#define ER_CR0 40
#define ER_CR3 44
#endif
