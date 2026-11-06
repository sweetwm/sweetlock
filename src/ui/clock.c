#include "ui/clock.h"

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>

#include "log.h"
#include "utf8.h"

static bool shows_seconds(const char *format) {
	if (!format) {
		return false;
	}
	struct tm tm = {.tm_year = 126, .tm_mday = 1};
	char first[CLOCK_TEXT_SIZE] = "";
	char second[CLOCK_TEXT_SIZE] = "";
	size_t a = strftime(first, sizeof first, format, &tm);
	tm.tm_sec = 1;
	size_t b = strftime(second, sizeof second, format, &tm);
	return a != b || memcmp(first, second, a) != 0;
}

static bool arm(struct clock *clock, time_t now) {
	time_t step = clock->seconds ? 1 : 60;
	struct itimerspec spec = {
		.it_value.tv_sec = (now / step + 1) * step,
	};
	return timerfd_settime(clock->fd,
		       TFD_TIMER_ABSTIME | TFD_TIMER_CANCEL_ON_SET, &spec,
		       NULL) == 0;
}

// a format that gives nothing, or text that is not plain UTF-8, shows empty
static bool format(char *text, const char *format, const struct tm *tm) {
	char fresh[CLOCK_TEXT_SIZE] = "";
	size_t length = format ? strftime(fresh, sizeof fresh, format, tm) : 0;
	if (!utf8_is_plain(fresh, length)) {
		fresh[0] = '\0';
	}
	bool changed = strcmp(fresh, text) != 0;
	memcpy(text, fresh, sizeof fresh);
	return changed;
}

static bool refresh(struct clock *clock, bool *changed) {
	struct timespec now = {0};
	clock_gettime(CLOCK_REALTIME, &now);
	// glibc rereads /etc/localtime here when it changed, so a new zone
	// shows by the next tick
	tzset();
	struct tm tm = {0};
	if (!localtime_r(&now.tv_sec, &tm)) {
		tm = (struct tm){0};
	}
	bool time = format(clock->time, clock->time_format, &tm);
	bool date = format(clock->date, clock->date_format, &tm);
	*changed = time || date;
	return arm(clock, now.tv_sec);
}

bool clock_start(
	struct clock *clock, const char *time_format, const char *date_format) {
	*clock = (struct clock){
		.time_format = time_format,
		.date_format = date_format,
		.seconds = shows_seconds(time_format) ||
			   shows_seconds(date_format),
	};
	clock->fd = timerfd_create(CLOCK_REALTIME, TFD_CLOEXEC | TFD_NONBLOCK);
	bool changed = false;
	if (clock->fd < 0 || !refresh(clock, &changed)) {
		log_error("failed to start the clock timer: %m");
		clock_stop(clock);
		return false;
	}
	if (time_format && !clock->time[0]) {
		log_warn("clock.format gives no text");
	}
	if (date_format && !clock->date[0]) {
		log_warn("date.format gives no text");
	}
	return true;
}

bool clock_update(struct clock *clock) {
	uint64_t expirations = 0;
	if (read(clock->fd, &expirations, sizeof expirations) < 0 &&
		errno == EAGAIN) {
		return false;
	}
	// anything else, ECANCELED from a wall-clock jump included, re-aligns
	bool changed = false;
	if (!refresh(clock, &changed)) {
		log_error("failed to re-arm the clock timer: %m");
	}
	return changed;
}

void clock_stop(struct clock *clock) {
	if (clock->fd >= 0) {
		close(clock->fd);
	}
	*clock = (struct clock){.fd = -1};
}
