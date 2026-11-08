#include "log.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

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
	// one write per line, so the locker's and the helper's lines never mix
	char line[LOG_LINE_MAX];
	int prefix =
		snprintf(line, sizeof line, "%s.%06ldZ %s sweetlock: ", stamp,
			now.tv_nsec / 1000, level_labels[level]);
	size_t length = prefix > 0 ? (size_t)prefix : 0;

	va_list args;
	va_start(args, fmt);
	errno = saved;
	int message = vsnprintf(line + length, sizeof line - length, fmt, args);
	va_end(args);
	// the newline takes the place of vsnprintf's NUL
	size_t room = sizeof line - length - 1;
	if (message > 0 && (size_t)message > room) {
		length += room;
		line[length - 3] = line[length - 2] = line[length - 1] = '.';
	} else if (message > 0) {
		length += (size_t)message;
	}
	line[length++] = '\n';
	for (size_t done = 0; done < length;) {
		ssize_t written =
			write(STDERR_FILENO, line + done, length - done);
		if (written < 0 && errno == EINTR) {
			continue;
		}
		if (written <= 0) {
			break;
		}
		done += (size_t)written;
	}
	errno = saved;
}
