#include "app/screen.h"

#include <stdint.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>

#include "input/input.h"
#include "log.h"
#include "wayland/display.h"
#include "wayland/power.h"

enum { NS_PER_S = 1000000000 };

bool screen_init(struct screen *screen, int32_t hide_s, int32_t off_s) {
	*screen = (struct screen){
		.fd = -1,
		.hide_ns = (int64_t)hide_s * NS_PER_S,
		.off_ns = (int64_t)off_s * NS_PER_S,
	};
	if (hide_s == 0 && off_s == 0) {
		return true;
	}
	screen->fd =
		timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
	if (screen->fd < 0) {
		log_error("failed to create the idle timer: %m");
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
		log_warn("failed to time the idle screen: %m");
	}
}

static int64_t idle_ns(const struct input *input) {
	struct timespec now = {0};
	clock_gettime(CLOCK_MONOTONIC, &now);
	return (int64_t)now.tv_sec * NS_PER_S + now.tv_nsec - input->active_ns;
}

static void schedule(
	struct screen *screen, const struct input *input, int64_t idle) {
	int64_t next = INT64_MAX;
	if (screen->hide_ns > idle && !input->quiet) {
		next = screen->hide_ns;
	}
	if (screen->off_ns > idle && !input->screens_off &&
		screen->off_ns < next) {
		next = screen->off_ns;
	}
	if (screen->fd >= 0 && next != INT64_MAX) {
		arm(screen, next - idle);
	}
}

void screen_start(
	struct screen *screen, struct display *display, struct input *input) {
	if (screen->off_ns > 0 && !power_start(display)) {
		screen->off_ns = 0;
	}
	input_touch(input);
	schedule(screen, input, 0);
}

// an attempt in flight holds both, and its end counts as input
void screen_expire(struct screen *screen, struct display *display,
	struct input *input, bool held) {
	uint64_t expirations = 0;
	if (read(screen->fd, &expirations, sizeof expirations) !=
		(ssize_t)sizeof expirations) {
		return;
	}
	int64_t idle = held ? 0 : idle_ns(input);
	if (screen->hide_ns > 0 && idle >= screen->hide_ns && !input->quiet) {
		log_info("no input for %d s; hiding the login",
			(int)(screen->hide_ns / NS_PER_S));
		input_hide(input);
	}
	if (screen->off_ns > 0 && idle >= screen->off_ns &&
		!input->screens_off) {
		log_info("no input for %d s; turning the screens off",
			(int)(screen->off_ns / NS_PER_S));
		input_clear(input);
		power_set(display, false);
	}
	schedule(screen, input, idle);
}

void screen_wake(
	struct screen *screen, struct display *display, struct input *input) {
	log_info("waking the screens");
	power_set(display, true);
	schedule(screen, input, idle_ns(input));
}

void screen_show(struct screen *screen, const struct input *input) {
	schedule(screen, input, idle_ns(input));
}
