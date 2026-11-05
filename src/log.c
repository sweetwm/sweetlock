#include "log.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char *const level_names[] = {"error", "warn", "info", "debug"};
static const char *const level_labels[] = {"ERROR", " WARN", " INFO", "DEBUG"};

static enum log_level threshold = LOG_INFO;

bool log_init(void) {
	const char *value = getenv("SWEETLOCK_LOG");
	if (!value) {
		return true;
	}
	for (size_t i = 0; i <= LOG_DEBUG; i++) {
		if (strcmp(value, level_names[i]) == 0) {
			threshold = (enum log_level)i;
			return true;
		}
	}
	return false;
}

void log_at(enum log_level level, const char *fmt, ...) {
	if (level > threshold) {
		return;
	}
	// callers may log with %m, so errno must survive the timestamp
	int saved = errno;
	struct timespec now = {0};
	struct tm tm = {0};
	char stamp[32] = "";
	clock_gettime(CLOCK_REALTIME, &now);
	if (gmtime_r(&now.tv_sec, &tm)) {
		strftime(stamp, sizeof stamp, "%Y-%m-%dT%H:%M:%S", &tm);
	}
	fprintf(stderr, "%s.%06ldZ %s sweetlock: ", stamp, now.tv_nsec / 1000,
		level_labels[level]);

	va_list args;
	va_start(args, fmt);
	errno = saved;
	vfprintf(stderr, fmt, args);
	va_end(args);
	fputc('\n', stderr);
	errno = saved;
}
