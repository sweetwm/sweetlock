#include "app.h"

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/signalfd.h>
#include <unistd.h>
#include <wayland-client.h>

#include "auth/auth.h"
#include "input/input.h"
#include "log.h"
#include "wayland/display.h"

enum {
	FD_WAYLAND,
	FD_SIGNAL,
	FD_AUTH,
	FD_REPEAT,
	FD_COUNT,
};

struct app {
	struct auth auth;
	struct input input;
	struct display display;
	int signal_fd;
};

enum signal_action {
	SIGNAL_NONE,
	SIGNAL_UNLOCK,
	SIGNAL_EXIT,
};

static void signal_set(sigset_t *set) {
	sigemptyset(set);
	sigaddset(set, SIGTERM);
	sigaddset(set, SIGINT);
	sigaddset(set, SIGUSR1);
}

bool app_protect(void) {
	if (prctl(PR_SET_DUMPABLE, 0, 0, 0, 0) != 0) {
		log_error("failed to make the process undumpable: %m");
		return false;
	}
	sigset_t set;
	signal_set(&set);
	if (sigprocmask(SIG_BLOCK, &set, NULL) != 0) {
		log_error("failed to block signals: %m");
		return false;
	}
	return true;
}

// SIGTERM and SIGINT leave the session locked, as the protocol requires of
// a locker that exits without unlocking
static enum signal_action read_signals(int fd) {
	enum signal_action action = SIGNAL_NONE;
	struct signalfd_siginfo info;
	while (read(fd, &info, sizeof info) == (ssize_t)sizeof info) {
		const char *name = sigabbrev_np((int)info.ssi_signo);
		if (info.ssi_signo == SIGUSR1) {
			log_info("received SIGUSR1; unlocking");
			if (action == SIGNAL_NONE) {
				action = SIGNAL_UNLOCK;
			}
		} else {
			log_info("received SIG%s; exiting without unlocking",
				name ? name : "?");
			action = SIGNAL_EXIT;
		}
	}
	return action;
}

// false when a signal asks the loop to exit
static bool handle_signals(struct display *display, int fd) {
	enum signal_action action = read_signals(fd);
	if (action == SIGNAL_UNLOCK) {
		lock_unlock(&display->lock);
	}
	return action != SIGNAL_EXIT;
}

// false when the helper is gone, since nothing could unlock after that
static bool handle_auth(struct app *app) {
	enum auth_result result = auth_read(&app->auth);
	if (result == AUTH_SUCCESS) {
		lock_unlock(&app->display.lock);
	}
	return result != AUTH_LOST;
}

// false when the loop must exit without unlocking
static bool handle_fds(struct app *app, const struct pollfd *fds) {
	if ((fds[FD_SIGNAL].revents & POLLIN) &&
		!handle_signals(&app->display, app->signal_fd)) {
		return false;
	}
	if (fds[FD_AUTH].revents && !handle_auth(app)) {
		return false;
	}
	if (fds[FD_REPEAT].revents & POLLIN) {
		input_repeat(&app->input);
	}
	return true;
}

static bool stopping(const struct display *display) {
	return display->failed || display->lock.state == LOCK_REFUSED ||
	       display->lock.state == LOCK_UNLOCKING;
}

// a queued unlock wins over any failure, and ends only after the
// compositor has processed it
static int stop(struct display *display) {
	if (display->lock.state != LOCK_UNLOCKING) {
		return EXIT_FAILURE;
	}
	if (wl_display_roundtrip(display->wl) < 0) {
		display_log_lost(display);
		return EXIT_FAILURE;
	}
	log_info("unlocked");
	return EXIT_SUCCESS;
}

static int run(struct app *app) {
	struct display *display = &app->display;
	struct wl_display *wl = display->wl;
	struct pollfd fds[FD_COUNT] = {
		[FD_WAYLAND] = {.fd = wl_display_get_fd(wl)},
		[FD_SIGNAL] = {.fd = app->signal_fd, .events = POLLIN},
		[FD_AUTH] = {.fd = app->auth.fd, .events = POLLIN},
		[FD_REPEAT] = {.fd = app->input.repeat_fd, .events = POLLIN},
	};

	for (;;) {
		while (wl_display_prepare_read(wl) != 0) {
			if (wl_display_dispatch_pending(wl) < 0) {
				goto lost;
			}
		}
		// every dispatch and signal is acted on here before blocking
		if (stopping(display)) {
			wl_display_cancel_read(wl);
			return stop(display);
		}
		fds[FD_WAYLAND].events = POLLIN;
		if (wl_display_flush(wl) < 0) {
			if (errno != EAGAIN) {
				wl_display_cancel_read(wl);
				goto lost;
			}
			fds[FD_WAYLAND].events |= POLLOUT;
		}

		if (poll(fds, FD_COUNT, -1) < 0) {
			wl_display_cancel_read(wl);
			if (errno == EINTR) {
				continue;
			}
			log_error("poll failed: %m");
			return EXIT_FAILURE;
		}

		if (fds[FD_WAYLAND].revents & (POLLIN | POLLERR | POLLHUP)) {
			if (wl_display_read_events(wl) < 0) {
				goto lost;
			}
		} else {
			wl_display_cancel_read(wl);
		}
		if (wl_display_dispatch_pending(wl) < 0) {
			goto lost;
		}

		if (!handle_fds(app, fds)) {
			return EXIT_FAILURE;
		}
	}

lost:
	display_log_lost(display);
	return EXIT_FAILURE;
}

static bool open_signals(struct app *app) {
	sigset_t set;
	signal_set(&set);
	app->signal_fd = signalfd(-1, &set, SFD_CLOEXEC | SFD_NONBLOCK);
	if (app->signal_fd < 0) {
		log_error("failed to monitor signals: %m");
		return false;
	}
	return true;
}

int app_run(const struct config *config, const struct timespec *start) {
	struct app app = {
		.auth = {.pid = -1, .fd = -1},
		.input = {.repeat_fd = -1},
		.signal_fd = -1,
	};
	int status = EXIT_FAILURE;
	// the helper forks first, before any thread or descriptor it must not
	// inherit
	if (auth_start(&app.auth) && open_signals(&app) &&
		input_init(&app.input, &app.auth) &&
		display_connect(&app.display, config, &app.input) &&
		display_lock(&app.display, start)) {
		status = run(&app);
	}
	display_disconnect(&app.display);
	input_finish(&app.input);
	if (app.signal_fd >= 0) {
		close(app.signal_fd);
	}
	auth_stop(&app.auth);
	return status;
}
