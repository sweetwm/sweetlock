#ifndef SWEETLOCK_APP_H
#define SWEETLOCK_APP_H

#include <stdbool.h>
#include <time.h>

#include "config.h"

bool app_protect(void);

int app_run(const struct config *config, const struct timespec *start);

#endif
