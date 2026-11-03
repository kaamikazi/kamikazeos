/* Host-only fixture over the actual registry, log and health implementations.
   No ports, disk, or CPU privilege instructions are executed. */
#include "../kernel/src/ghostattach.c"
#include "../kernel/src/boot_snapshot.c"

static struct recovery_memory test_memory;
const struct recovery_memory *recovery_memory_get(void) { return &test_memory; }
int keyboard_present(void) { return 1; }
int serial_present(void) { return 1; }
void serial_print(const char *s) { (void)s; }
void serial_print_field(const char *s) { (void)s; }

int test_blocked_exact_verified_fallback(void)
{
    errors_init();
    devices[5].required_driver = "generic_storage";
    ghostattach_scan(0);
    struct health_breakdown health;
    boot_snapshot_health_now(&health);
    return devices[5].status == DEVST_FALLBACK &&
           drivers[3].status == DRVST_BLOCKED_UNVERIFIED &&
           health.missing_penalty == -20 && health.warning_penalty == -5 &&
           health.score == 75;
}

int test_blocked_driver_without_fallback_scored_once(void)
{
    errors_init();
    ghostattach_scan(0);
    struct health_breakdown health;
    boot_snapshot_health_now(&health);
    return devices[3].status == DEVST_MISSING_DRIVER &&
           health.missing_penalty == -20 && health.warning_penalty == 0 &&
           health.score == 80;
}

int test_verified_exact_has_no_security_penalty(void)
{
    errors_init();
    ghostattach_scan(0);
    return devices[0].status == DEVST_ATTACHED &&
           error_health_count(HEALTH_PRODUCTION, SEV_WARNING) == 0;
}
