#ifndef SWEETLOCK_APP_H
#define SWEETLOCK_APP_H

#include <stdbool.h>
#include <time.h>

#include "args.h"
#include "config.h"

bool app_protect(void);

int app_daemonize(void);

// writes the one report byte and closes fd
void app_report_locked(int fd);

// draws the lock scene to a PNG without connecting to Wayland
int app_preview(const struct config *config, const struct preview *preview);

int app_run(const struct config *config, const struct timespec *start,
	int locked_fd);

#endif
