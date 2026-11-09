#include "app/screen.h"

#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>

#include "input/input.h"
#include "log.h"
#include "wayland/display.h"
#include "wayland/power.h"

enum { NS_PER_S = 1000000000 };

bool screen_init(struct screen *screen, int32_t timeout_s) {
	*screen = (struct screen){
		.fd = -1,
		.timeout_ns = (int64_t)timeout_s * NS_PER_S,
	};
	if (timeout_s == 0) {
		return true;
	}
	screen->fd =
		timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
	if (screen->fd < 0) {
		log_error("failed to create the screen-off timer: %m");
		return false;
	}
	return true;
}

void screen_finish(struct screen *screen) {
	if (screen->fd >= 0) {
		close(screen->fd);
	}
	*screen = (struct screen){.fd = -1};
}

// without the timer the screens just stay on
static void arm(struct screen *screen, int64_t after_ns) {
	const struct itimerspec spec = {
		.it_value = {.tv_sec = (time_t)(after_ns / NS_PER_S),
			.tv_nsec = (long)(after_ns % NS_PER_S)},
	};
	if (timerfd_settime(screen->fd, 0, &spec, NULL) != 0) {
		log_warn("failed to time the screen-off: %m");
	}
}

void screen_start(
	struct screen *screen, struct display *display, struct input *input) {
	if (screen->fd >= 0 && power_start(display)) {
		input_touch(input);
		arm(screen, screen->timeout_ns);
	}
}

// an attempt in flight holds the screens on, and its end counts as input
void screen_expire(struct screen *screen, struct display *display,
	struct input *input, bool busy) {
	uint64_t expirations = 0;
	if (read(screen->fd, &expirations, sizeof expirations) !=
		(ssize_t)sizeof expirations) {
		return;
	}
	struct timespec now = {0};
	clock_gettime(CLOCK_MONOTONIC, &now);
	int64_t idle =
		(int64_t)now.tv_sec * NS_PER_S + now.tv_nsec - input->active_ns;
	if (busy || idle < screen->timeout_ns) {
		arm(screen,
			busy ? screen->timeout_ns : screen->timeout_ns - idle);
		return;
	}
	log_info("no input for %d s; turning the screens off",
		(int)(screen->timeout_ns / NS_PER_S));
	input_clear(input);
	power_set(display, false);
}

void screen_wake(struct screen *screen, struct display *display) {
	log_info("waking the screens");
	power_set(display, true);
	if (screen->fd >= 0) {
		arm(screen, screen->timeout_ns);
	}
}
