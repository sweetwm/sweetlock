#ifndef SWEETLOCK_APP_H
#define SWEETLOCK_APP_H

#include <stdbool.h>
#include <time.h>

#include "config.h"

bool app_protect(void);

int app_daemonize(void);

// writes the one report byte and closes fd
void app_report_locked(int fd);

int app_run(const struct config *config, const struct timespec *start,
	int locked_fd);

#endif
