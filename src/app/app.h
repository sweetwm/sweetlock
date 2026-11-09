#ifndef SWEETLOCK_APP_APP_H
#define SWEETLOCK_APP_APP_H

#include <stdbool.h>
#include <time.h>

#include "config/args.h"
#include "config/config.h"

bool app_protect(void);

int app_daemonize(void);

// writes the one report byte and closes fd
void app_report_locked(int fd);

// logs every problem it finds, without connecting to Wayland or PAM
int app_check(const struct config *config, bool config_ok);

// notice is the config's status line, or empty
int app_preview(const struct config *config, const char *notice,
	const struct preview *preview);

int app_run(const struct config *config, const char *notice,
	const struct timespec *start, int locked_fd);

#endif
