#include "app/app.h"

#include <errno.h>
#include <malloc.h>
#include <poll.h>
#include <stdlib.h>
#include <unistd.h>
#include <wayland-client.h>

#include "app/screen.h"
#include "app/signals.h"
#include "app/worker.h"
#include "auth/auth.h"
#include "input/input.h"
#include "input/password.h"
#include "log.h"
#include "render/text.h"
#include "ui/clock.h"
#include "ui/field.h"
#include "ui/ui.h"
#include "ui/user.h"
#include "wayland/backdrop.h"
#include "wayland/display.h"
#include "wayland/power.h"

enum {
	FD_WAYLAND,
	FD_SIGNAL,
	FD_AUTH,
	FD_AUTH_TIMER,
	FD_REPEAT,
	FD_CLOCK,
	FD_FIELD,
	FD_WORKER,
	FD_SCREEN,
	FD_COUNT,
};

enum ui_stage {
	STAGE_WAITING,
	STAGE_STARTED,
	// the first frame is committed and its time logged
	STAGE_SHOWN,
};

struct app {
	struct auth auth;
	struct input input;
	struct display display;
	struct clock clock;
	struct field field;
	struct worker worker;
	struct screen screen;
	const struct config *config;
	const char *notice;
	struct user user;
	struct ui_content content;
	enum ui_stage stage;
	bool avatar_requested;
	int signal_fd;
	// -f only: reported and closed once the lock is confirmed
	int locked_fd;
};

// false when a signal asks the loop to exit
static bool handle_signals(struct display *display, int fd) {
	enum signal_action action = signals_read(fd);
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
	} else if (result == AUTH_FAILURE) {
		input_touch(&app->input);
		field_fail(&app->field);
		display_mark_dirty(&app->display, UI_LOGIN);
	} else if (result == AUTH_NOTICE) {
		display_mark_dirty(&app->display, UI_LOGIN);
	}
	return result != AUTH_LOST;
}

static enum field_state field_now(const struct app *app) {
	return field_state(
		&app->field, app->input.password->length, app->auth.busy);
}

static void handle_field(struct app *app) {
	enum field_state before = field_now(app);
	field_expire(&app->field);
	if (field_now(app) != before) {
		display_mark_dirty(&app->display, UI_LOGIN);
	}
}

// a ready background is attached by the next render pass
static void collect(struct app *app) {
	struct worker_job *job = worker_collect(&app->worker);
	while (job) {
		// a background job may go straight back to the worker
		struct worker_job *next = job->next;
		if (job == &app->content.avatar.job) {
			if (avatar_collect(
				    &app->content.avatar, &app->worker)) {
				display_mark_dirty(&app->display, UI_LOGIN);
			}
		} else {
			backdrop_collect(&app->display, &app->worker, job);
		}
		job = next;
	}
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
	if ((fds[FD_AUTH_TIMER].revents & POLLIN) && auth_expire(&app->auth)) {
		display_mark_dirty(&app->display, UI_LOGIN);
	}
	if (fds[FD_REPEAT].revents & POLLIN) {
		input_repeat(&app->input);
	}
	if ((fds[FD_CLOCK].revents & POLLIN) && clock_update(&app->clock)) {
		display_mark_dirty(&app->display, UI_CLOCK);
	}
	if (fds[FD_FIELD].revents & POLLIN) {
		handle_field(app);
	}
	if (fds[FD_WORKER].revents & POLLIN) {
		collect(app);
	}
	if (fds[FD_SCREEN].revents & POLLIN) {
		screen_expire(&app->screen, &app->display, &app->input,
			app->auth.busy);
	}
	return true;
}

// a cancel that found no new helper sets auth.lost from a key handler
static bool stopping(const struct app *app) {
	const struct display *display = &app->display;
	return display->failed || display->lock.state == LOCK_REFUSED ||
	       display->lock.state == LOCK_UNLOCKING || app->auth.lost;
}

static int stop(struct display *display) {
	if (display->lock.state != LOCK_UNLOCKING) {
		return EXIT_FAILURE;
	}
	power_set(display, true);
	if (wl_display_roundtrip(display->wl) < 0) {
		display_log_lost(display);
		return EXIT_FAILURE;
	}
	log_info("unlocked");
	return EXIT_SUCCESS;
}

static void report_locked(struct app *app) {
	enum lock_state state = app->display.lock.state;
	// an unlock queued before locked goes straight to unlocking
	if (app->locked_fd >= 0 &&
		(state == LOCK_LOCKED || state == LOCK_UNLOCKING)) {
		app_report_locked(app->locked_fd);
		app->locked_fd = -1;
	}
}

// started with the lock request, so a cached background is read while the
// compositor confirms the lock
static void start_worker(struct app *app) {
	(void)mallopt(M_MMAP_THRESHOLD, 1 << 20);
	worker_start(&app->worker);
	app->content = (struct ui_content){
		.config = app->config,
		.input = &app->input,
		.auth = &app->auth,
		.field = &app->field,
		.worker = &app->worker,
		.user = &app->user,
		.notice = app->notice,
	};
}

// after the backgrounds, so their jobs reach the worker first
static void load_avatar(struct app *app) {
	if (!app->avatar_requested) {
		ui_content_load_avatar(&app->content);
		app->avatar_requested = true;
	}
}

static void start_ui(struct app *app) {
	display_show_backgrounds(&app->display, &app->content);
	load_avatar(app);
	ui_content_start(&app->content, &app->clock);
	screen_start(&app->screen, &app->display, &app->input);
	app->input.ready = true;
	app->stage = STAGE_STARTED;
}

// drawing from the loop makes a burst of events cost one frame
static void render(struct app *app) {
	struct display *display = &app->display;
	// a background ready before locked can be the first frame shown
	if (display->lock.state == LOCK_REQUESTED) {
		display_show_backgrounds(display, &app->content);
		if (display_mapped(display)) {
			load_avatar(app);
		}
	}
	if (display->lock.state != LOCK_LOCKED) {
		return;
	}
	if (app->stage == STAGE_WAITING) {
		start_ui(app);
	}
	if (app->input.wake) {
		app->input.wake = false;
		screen_wake(&app->screen, display);
	}
	if (app->input.changed) {
		app->input.changed = false;
		display_mark_dirty(display, UI_LOGIN);
	}
	if (app->input.layout_changed) {
		app->input.layout_changed = false;
		display_mark_dirty(display,
			app->config->input.layout == LAYOUT_CORNER ? UI_LAYOUT
								   : UI_LOGIN);
	}
	if (display_render(display, &app->content) &&
		app->stage == STAGE_STARTED) {
		log_info("first UI frame in %.1f ms",
			lock_elapsed_ms(&display->lock));
		app->stage = STAGE_SHOWN;
	}
}

// a full socket waits for POLLOUT; false when the connection is gone
static bool flush(struct wl_display *wl, short *events) {
	*events = POLLIN;
	if (wl_display_flush(wl) < 0) {
		if (errno != EAGAIN) {
			return false;
		}
		*events |= POLLOUT;
	}
	return true;
}

static void read_last_error(struct wl_display *wl) {
	int error = errno;
	if (error != EPIPE) {
		wl_display_cancel_read(wl);
	} else if (wl_display_read_events(wl) == 0) {
		(void)wl_display_dispatch_pending(wl);
	}
	errno = error;
}

static void late_fds(const struct app *app, struct pollfd *fds) {
	fds[FD_AUTH].fd = app->auth.fd;
	fds[FD_CLOCK].fd = app->clock.fd;
	fds[FD_WORKER].fd = app->worker.fd;
}

static int run(struct app *app) {
	struct display *display = &app->display;
	struct wl_display *wl = display->wl;
	struct pollfd fds[FD_COUNT] = {
		[FD_WAYLAND] = {.fd = wl_display_get_fd(wl)},
		[FD_SIGNAL] = {.fd = app->signal_fd, .events = POLLIN},
		[FD_AUTH] = {.events = POLLIN},
		[FD_AUTH_TIMER] = {.fd = app->auth.timer_fd, .events = POLLIN},
		[FD_REPEAT] = {.fd = app->input.repeat_fd, .events = POLLIN},
		[FD_CLOCK] = {.events = POLLIN},
		[FD_FIELD] = {.fd = app->field.timer_fd, .events = POLLIN},
		[FD_WORKER] = {.events = POLLIN},
		[FD_SCREEN] = {.fd = app->screen.fd, .events = POLLIN},
	};

	for (;;) {
		while (wl_display_prepare_read(wl) != 0) {
			if (wl_display_dispatch_pending(wl) < 0) {
				goto lost;
			}
		}
		// every dispatch and signal is acted on here before blocking
		report_locked(app);
		if (stopping(app)) {
			wl_display_cancel_read(wl);
			return stop(display);
		}
		render(app);
		late_fds(app, fds);
		if (!flush(wl, &fds[FD_WAYLAND].events)) {
			read_last_error(wl);
			goto lost;
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

int app_run(const struct config *config, const char *notice,
	const struct timespec *start, int locked_fd) {
	struct app app = {
		.auth = {.pid = -1, .fd = -1, .timer_fd = -1},
		.input = {.repeat_fd = -1},
		.clock = {.fd = -1},
		.field = {.timer_fd = -1},
		.worker = {.fd = -1},
		.screen = {.fd = -1},
		.config = config,
		.notice = notice,
		.signal_fd = -1,
		.locked_fd = locked_fd,
	};
	int status = EXIT_FAILURE;
	// one lookup for PAM and the UI, since with sssd or LDAP it can be slow
	user_lookup(&app.user, config->username.text);
	if (auth_start(&app.auth, app.user.login) &&
		signals_open(&app.signal_fd) && field_init(&app.field) &&
		input_init(&app.input, &app.auth, &app.field) &&
		screen_init(&app.screen, config->screen_off.timeout) &&
		display_connect(&app.display, config, &app.input) &&
		display_lock(&app.display, start)) {
		wl_display_flush(app.display.wl);
		text_prepare();
		start_worker(&app);
		status = run(&app);
	}
	// the worker first: it may be drawing into a backdrop's buffer
	worker_stop(&app.worker);
	display_disconnect(&app.display);
	ui_content_finish(&app.content);
	clock_stop(&app.clock);
	field_finish(&app.field);
	screen_finish(&app.screen);
	input_finish(&app.input);
	if (app.signal_fd >= 0) {
		close(app.signal_fd);
	}
	if (app.locked_fd >= 0) {
		close(app.locked_fd);
	}
	auth_stop(&app.auth);
	return status;
}
