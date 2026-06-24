#include "locky/log.h"
#include <stdarg.h>
#include <stdio.h>

static enum locky_log_level current_level = LOCKY_ERROR;

static const char *level_name(enum locky_log_level level) {
	switch (level) {
	case LOCKY_ERROR:
		return "ERROR";
	case LOCKY_INFO:
		return "INFO";
	case LOCKY_DEBUG:
		return "DEBUG";
	default:
		return "";
	}
}

void locky_log_init(enum locky_log_level level) {
	current_level = level;
}

void locky_log(enum locky_log_level level, const char *fmt, ...) {
	if (level == LOCKY_SILENT || level > current_level) {
		return;
	}

	fprintf(stderr, "locky: [%s] ", level_name(level));

	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);

	fputc('\n', stderr);
}
