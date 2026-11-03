#ifndef KMZ_APP_POLICY_H
#define KMZ_APP_POLICY_H

struct app;

int app_policy_can_run(const struct app *app, const char *requested_name,
                       const char **reason);
const char *app_policy_mode(void);

#endif
