/*
 * string.h — minimal string/memory helpers.
 *
 * A kernel is "freestanding": there is NO C standard library here — no
 * printf, no strcmp, nothing. Anything we want, we write ourselves.
 */
#ifndef KMZ_STRING_H
#define KMZ_STRING_H

#include <stddef.h>
#include <stdint.h>

/* Required by the C ABI even when freestanding: the compiler emits calls
   to these for struct assignments and large initializations. */
void *memcpy(void *dst, const void *src, size_t n);
void *memset(void *dst, int value, size_t n);

size_t kstrlen(const char *s);
int    kstrcmp(const char *a, const char *b);          /* 0 if equal */
int    kstricmp(const char *a, const char *b);         /* 0 if equal, case-insensitive */
void   kstrcpy(char *dst, const char *src, size_t cap); /* always 0-terminates */
void  *kmemset(void *dst, int value, size_t n);
void   kitoa(uint32_t value, char *out, int base);      /* uint -> text */

#endif /* KMZ_STRING_H */
