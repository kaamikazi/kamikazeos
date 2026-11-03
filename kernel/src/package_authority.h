/* RAM-only joint authority. No on-disk field, epoch or compatibility fallback. */
#ifndef KMZ_PACKAGE_AUTHORITY_H
#define KMZ_PACKAGE_AUTHORITY_H
#include "package_state.h"

int package_authority_trusted(void);
const char *package_authority_state(void);
const char *package_authority_reason(void);
void package_authority_note_uncertain(const char *reason);
int package_authority_allow_state_write(void);
int package_authority_allow_slot_write(void);
int package_authority_recovery_status(const struct pkg_pstate *s);
int package_authority_begin_reset(void);
void package_authority_end_reset(void);
int package_authority_reset_active(void);
#endif
