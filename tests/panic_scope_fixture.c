/* Compile the actual return-safe wrapper with a deliberately returning trigger. */
#include "../kernel/src/panic_test.c"

static int pending, observed, reported;
void kpanic_expect_intentional(void) { pending = 1; }
void kpanic_cancel_intentional(void) { pending = 0; }
void error_log(enum err_severity severity, const char *code,
               const char *component, const char *message, const char *hint)
{
    (void)component; (void)message; (void)hint;
    reported = severity == SEV_ERROR && code[0] == 'T' && pending == 0;
}
static void returning_trigger(void) { observed = pending; }

int test_returning_trigger_clears_classification(void)
{
    kpanic_run_intentional_test(returning_trigger);
    return observed == 1 && pending == 0 && reported == 1;
}

int test_null_trigger_does_not_arm_classification(void)
{
    kpanic_run_intentional_test(0);
    return pending == 0 && reported == 0;
}
