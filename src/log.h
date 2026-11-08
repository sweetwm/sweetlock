#ifndef SWEETLOCK_LOG_H
#define SWEETLOCK_LOG_H

#include <stdbool.h>

// a longer line is cut and ends in "..."
enum { LOG_LINE_MAX = 8192 };

enum log_level {
	LOG_ERROR,
	LOG_WARN,
	LOG_INFO,
	LOG_DEBUG,
};

// reads SWEETLOCK_LOG; false means it named no known level
bool log_init(void);

void log_at(enum log_level level, const char *fmt, ...)
	__attribute__((format(printf, 2, 3)));

#define log_error(...) log_at(LOG_ERROR, __VA_ARGS__)
#define log_warn(...) log_at(LOG_WARN, __VA_ARGS__)
#define log_info(...) log_at(LOG_INFO, __VA_ARGS__)
#define log_debug(...) log_at(LOG_DEBUG, __VA_ARGS__)

#endif
