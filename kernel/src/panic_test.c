#include "panic.h"
#include "errors.h"

void kpanic_run_intentional_test(void (*trigger)(void))
{
    if (!trigger)
        return;
    kpanic_expect_intentional();
    trigger();
    kpanic_cancel_intentional();
    error_log(SEV_ERROR, "TEST-RETURN-001", "FaultTest",
              "Intentional fault trigger unexpectedly returned",
              "test classification cleared - inspect trigger implementation");
}
