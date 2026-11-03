#include "app_policy.h"
#include "app.h"

int app_policy_can_run(const struct app *app, const char *requested_name,
                       const char **reason)
{
    (void)requested_name;
    if (!app) {
        if (reason)
            *reason = "unknown app - only fixed built-in apps can run";
        return 0;
    }
    if (!app->entry) {
        if (reason)
            *reason = "app has no compile-time registered entry";
        return 0;
    }
    if (app->status == APP_BLOCKED) {
        if (reason)
            *reason = "app is blocked by policy";
        return 0;
    }
    if (reason)
        *reason = "registered compile-time app entry allowed";
    return 1;
}

const char *app_policy_mode(void)
{
    return "kernel_managed_builtin_apps";
}
