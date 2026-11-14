#ifndef SWEETLOCK_AUTH_SERVICE_H
#define SWEETLOCK_AUTH_SERVICE_H

#include <security/pam_appl.h>
#include <stdbool.h>

#define SERVICE_NAME "sweetlock"

// what the lock screen shows when the service is missing
#define SERVICE_MISSING                                                        \
	"PAM service \"" SERVICE_NAME "\" is missing; the password may be "    \
	"refused"

// a stack PAM could not load, such as an include of a missing service
#define SERVICE_BROKEN                                                         \
	"PAM could not load service \"" SERVICE_NAME "\"; run sweetlock "      \
	"--check"

#define SERVICE_MISSING_LOG                                                    \
	"no PAM service \"" SERVICE_NAME "\"; PAM falls back to \"other\", "   \
	"which usually refuses every password"

// whether dir holds the service as a regular file PAM could open
bool service_in(const char *dir);

bool service_found(void);

// the test build's service when built for tests; returns a PAM status
int service_start(
	const char *user, const struct pam_conv *conv, pam_handle_t **pam);

#endif
