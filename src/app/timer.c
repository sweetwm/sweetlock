#include "app/timer.h"
#include <errno.h>
#include <stdint.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>

#define NSEC_PER_SEC 1000000000ULL
#define NSEC_PER_MIN (60ULL * NSEC_PER_SEC)

static bool arm_next_minute(int fd) {
	struct timespec now = {0};
	if (clock_gettime(CLOCK_REALTIME, &now) != 0) {
		return false;
	}

	uint64_t minute_ns = (uint64_t)(now.tv_sec % 60) * NSEC_PER_SEC +
			     (uint64_t)now.tv_nsec;
	uint64_t delay_ns = NSEC_PER_MIN - minute_ns;
	if (delay_ns == 0) {
		delay_ns = NSEC_PER_MIN;
	}

	struct itimerspec spec = {0};
	spec.it_value.tv_sec = (time_t)(delay_ns / NSEC_PER_SEC);
	spec.it_value.tv_nsec = (long)(delay_ns % NSEC_PER_SEC);
	spec.it_interval.tv_sec = 60;

	return timerfd_settime(fd, 0, &spec, NULL) == 0;
}

bool locky_minute_timer_init(int *fd) {
	*fd = timerfd_create(CLOCK_REALTIME, TFD_CLOEXEC | TFD_NONBLOCK);
	if (*fd < 0) {
		return false;
	}
	if (!arm_next_minute(*fd)) {
		locky_minute_timer_finish(fd);
		return false;
	}
	return true;
}

void locky_minute_timer_drain(int fd) {
	uint64_t expirations = 0;
	while (read(fd, &expirations, sizeof(expirations)) > 0) {
	}
	if (errno != EAGAIN && errno != EWOULDBLOCK) {
		(void)arm_next_minute(fd);
	}
}

void locky_minute_timer_finish(int *fd) {
	if (*fd >= 0) {
		close(*fd);
		*fd = -1;
	}
}
