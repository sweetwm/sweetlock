#ifndef SWEETLOCK_UI_CLOCK_H
#define SWEETLOCK_UI_CLOCK_H

#include <stdbool.h>

enum { CLOCK_TEXT_SIZE = 128 };

// the time and the date, re-read on each minute, or second when a format
// shows seconds
struct clock {
	int fd;
	// NULL for a hidden line, whose text stays empty
	const char *time_format;
	const char *date_format;
	bool seconds;
	char time[CLOCK_TEXT_SIZE];
	char date[CLOCK_TEXT_SIZE];
};

// the formats must outlive the clock; logs failures
bool clock_start(
	struct clock *clock, const char *time_format, const char *date_format);

// call when fd is readable, true when a text changed
bool clock_update(struct clock *clock);

void clock_stop(struct clock *clock);

#endif
