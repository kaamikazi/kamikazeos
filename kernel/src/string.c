/* string.c — see string.h. Plain, obvious implementations on purpose. */
#include "string.h"

/*
 * Even in freestanding mode, the COMPILER ITSELF emits calls to memcpy /
 * memset for things like struct assignment (`*out = ring[i];`). There is
 * no libc to provide them, so the kernel must. These exact names and
 * signatures are required by the C ABI.
 */
void *memcpy(void *dst, const void *src, size_t n)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    while (n--)
        *d++ = *s++;
    return dst;
}

void *memset(void *dst, int value, size_t n)
{
    unsigned char *p = (unsigned char *)dst;
    while (n--)
        *p++ = (unsigned char)value;
    return dst;
}

size_t kstrlen(const char *s)
{
    size_t n = 0;
    while (s[n] != '\0')
        n++;
    return n;
}

int kstrcmp(const char *a, const char *b)
{
    /* Walk both strings while they match; stop at the first difference
       or at the end of either string. */
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

/* Lowercase one ASCII character (only A-Z change; everything else passes). */
static char to_lower(char c)
{
    return (c >= 'A' && c <= 'Z') ? (char)(c + ('a' - 'A')) : c;
}

/* Case-insensitive compare, so users can type `approve act-001`. */
int kstricmp(const char *a, const char *b)
{
    while (*a != '\0' && to_lower(*a) == to_lower(*b)) {
        a++;
        b++;
    }
    return (unsigned char)to_lower(*a) - (unsigned char)to_lower(*b);
}

void kstrcpy(char *dst, const char *src, size_t cap)
{
    size_t i = 0;
    if (cap == 0)
        return;
    /* Copy at most cap-1 characters so the terminator always fits. */
    while (i < cap - 1 && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

void *kmemset(void *dst, int value, size_t n)
{
    return memset(dst, value, n);
}

/*
 * kitoa — turn an unsigned number into text in the given base (10 or 16).
 * Digits come out backwards (least significant first), so we build the
 * string in a temp buffer and then reverse it. `out` must hold >= 12 bytes.
 */
void kitoa(uint32_t value, char *out, int base)
{
    static const char digits[] = "0123456789abcdef";
    char tmp[12];
    int i = 0;

    if (value == 0) {
        out[0] = '0';
        out[1] = '\0';
        return;
    }
    while (value > 0) {
        tmp[i++] = digits[value % (uint32_t)base];
        value /= (uint32_t)base;
    }
    /* Reverse into the caller's buffer. */
    int j = 0;
    while (i > 0)
        out[j++] = tmp[--i];
    out[j] = '\0';
}
