#ifndef KMZ_MEMORY_LAYOUT_H
#define KMZ_MEMORY_LAYOUT_H
#include "memory_policy.h"
void memory_layout_init(void);
const struct mp_layout *memory_layout_get(void);
int memory_layout_valid(void);
const char *memory_layout_error(void);
#endif
