/* Serial-number ordering for persisted 32-bit generations. */
#ifndef KMZ_GENERATION_H
#define KMZ_GENERATION_H

#include <stdint.h>

enum generation_order {
    GENERATION_EQUAL,
    GENERATION_NEWER,
    GENERATION_OLDER,
    GENERATION_AMBIGUOUS
};

static inline enum generation_order generation_compare(uint32_t a, uint32_t b)
{
    uint32_t distance = a - b;
    if (!distance)
        return GENERATION_EQUAL;
    if (distance == 0x80000000u)
        return GENERATION_AMBIGUOUS;
    return distance < 0x80000000u ? GENERATION_NEWER : GENERATION_OLDER;
}

#endif /* KMZ_GENERATION_H */
