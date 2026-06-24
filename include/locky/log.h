#ifndef LOCKY_LOG_H
#define LOCKY_LOG_H

enum locky_log_level {
	LOCKY_SILENT = 0,
	LOCKY_ERROR,
	LOCKY_INFO,
	LOCKY_DEBUG,
};

void locky_log_init(enum locky_log_level level);

void locky_log(enum locky_log_level level, const char *fmt, ...)
	__attribute__((format(printf, 2, 3)));

#endif
