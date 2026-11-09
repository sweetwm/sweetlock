#ifndef SWEETLOCK_AUTH_SERVICE_H
#define SWEETLOCK_AUTH_SERVICE_H

#include <stdbool.h>

#define SERVICE_NAME "sweetlock"

// what the lock screen shows when the service is missing
#define SERVICE_MISSING                                                        \
	"PAM service \"" SERVICE_NAME "\" is missing; the password may be "    \
	"refused"

#define SERVICE_MISSING_LOG                                                    \
	"no PAM service \"" SERVICE_NAME "\"; PAM falls back to \"other\", "   \
	"which usually refuses every password"

// whether dir holds the service as a regular file PAM could open
bool service_in(const char *dir);

bool service_found(void);

#endif
