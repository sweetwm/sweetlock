#include "input/input.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>
#include <xkbcommon/xkbcommon-compose.h>
#include <xkbcommon/xkbcommon.h>

#include "auth/auth.h"
#include "fs/path.h"
#include "input/keyboard.h"
#include "input/password.h"
#include "log.h"

// through our logger, so its lines never mix with the helper's
__attribute__((format(printf, 3, 0))) static void log_xkb(
	struct xkb_context *xkb, enum xkb_log_level level, const char *format,
	va_list args) {
	(void)xkb;
	char line[512];
	if (vsnprintf(line, sizeof line, format, args) < 0) {
		return;
	}
	line[strcspn(line, "\n")] = '\0';
	log_at(level <= XKB_LOG_LEVEL_ERROR ? LOG_WARN : LOG_DEBUG,
		"xkbcommon: %s", line);
}

bool input_init(struct input *input, struct auth *auth) {
	*input = (struct input){.auth = auth, .repeat_fd = -1};
	// compositor keymaps arrive compiled, so xkb never reads its include
	// paths
	input->xkb = xkb_context_new(XKB_CONTEXT_NO_DEFAULT_INCLUDES);
	if (!input->xkb) {
		log_error("failed to create the xkb context");
		return false;
	}
	xkb_context_set_log_fn(input->xkb, log_xkb);
	input->repeat_fd =
		timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
	if (input->repeat_fd < 0) {
		log_error("failed to create the key repeat timer: %m");
		return false;
	}
	input->password = password_create();
	return input->password != NULL;
}

void input_finish(struct input *input) {
	if (input->repeat_fd >= 0) {
		close(input->repeat_fd);
	}
	password_destroy(input->password);
	xkb_compose_table_unref(input->compose);
	xkb_context_unref(input->xkb);
	*input = (struct input){.repeat_fd = -1};
}

static bool user_files_regular(void) {
	char paths[3][PATH_MAX] = {{0}};
	const char *file = getenv("XCOMPOSEFILE");
	const char *xdg = getenv("XDG_CONFIG_HOME");
	const char *home = getenv("HOME");
	if (file) {
		snprintf(paths[0], PATH_MAX, "%s", file);
	}
	if (xdg && xdg[0] == '/') {
		path_join(paths[1], xdg, "/XCompose");
	} else if (home) {
		path_join(paths[1], home, "/.config/XCompose");
	}
	if (home) {
		path_join(paths[2], home, "/.XCompose");
	}
	for (size_t i = 0; i < 3; i++) {
		struct stat info;
		if (paths[i][0] && stat(paths[i], &info) == 0 &&
			!S_ISREG(info.st_mode)) {
			log_warn("%s is not a regular file", paths[i]);
			return false;
		}
	}
	return true;
}

// the locale xkbcommon documents for Compose
static const char *compose_locale(void) {
	const char *names[] = {"LC_ALL", "LC_CTYPE", "LANG"};
	for (size_t i = 0; i < 3; i++) {
		const char *value = getenv(names[i]);
		if (value && value[0]) {
			return value;
		}
	}
	return "C";
}

static void load_compose(struct input *input) {
	input->compose_loaded = true;
	const char *locale = compose_locale();
	struct timespec start = {0};
	struct timespec end = {0};
	clock_gettime(CLOCK_MONOTONIC, &start);
	if (user_files_regular()) {
		input->compose = xkb_compose_table_new_from_locale(
			input->xkb, locale, XKB_COMPOSE_COMPILE_NO_FLAGS);
	}
	clock_gettime(CLOCK_MONOTONIC, &end);
	if (!input->compose) {
		log_warn("no Compose table for locale \"%s\"; dead keys and "
			 "Compose type nothing",
			locale);
		return;
	}
	log_debug("Compose table for locale \"%s\" in %.1f ms", locale,
		(double)(end.tv_sec - start.tv_sec) * 1e3 +
			(double)(end.tv_nsec - start.tv_nsec) / 1e6);
}

static struct timespec from_ns(int64_t ns) {
	return (struct timespec){
		.tv_sec = (time_t)(ns / 1000000000),
		.tv_nsec = (long)(ns % 1000000000),
	};
}

static void stop_repeat(struct input *input) {
	input->repeat_keyboard = NULL;
	timerfd_settime(input->repeat_fd, 0, &(struct itimerspec){0}, NULL);
}

static void start_repeat(
	struct input *input, struct keyboard *keyboard, uint32_t key) {
	int32_t rate = 0;
	int32_t delay = 0;
	if (!keyboard_repeats(keyboard, key, &rate, &delay)) {
		stop_repeat(input);
		return;
	}
	// a zero it_value disarms the timer
	int64_t delay_ns = (int64_t)delay * 1000000;
	const struct itimerspec spec = {
		.it_value = from_ns(delay_ns > 0 ? delay_ns : 1),
		.it_interval = from_ns(1000000000 / rate),
	};
	if (timerfd_settime(input->repeat_fd, 0, &spec, NULL) != 0) {
		stop_repeat(input);
		return;
	}
	input->repeat_keyboard = keyboard;
	input->repeat_key = key;
}

static void submit(struct input *input) {
	stop_repeat(input);
	if (auth_submit(input->auth, input->password)) {
		log_debug("checking the password");
	}
	password_clear(input->password);
}

// a repeat never submits, so a held Enter is one attempt
static enum key_action handle(struct input *input, struct keyboard *keyboard,
	uint32_t key, bool repeat) {
	char text[KEY_TEXT_SIZE];
	enum key_action action =
		keyboard_translate(keyboard, key, text, sizeof text);
	struct password *password = input->password;
	bool changed = false;
	switch (action) {
	case KEY_TEXT:
	case KEY_COMPOSED:
		// input past the limit is dropped
		changed = password_append(password, text, strlen(text));
		break;
	case KEY_BACKSPACE:
		changed = password_backspace(password);
		break;
	case KEY_CLEAR:
		changed = password->length > 0;
		password_clear(password);
		break;
	case KEY_SUBMIT:
		if (!repeat) {
			submit(input);
			changed = true;
		}
		break;
	case KEY_NONE:
		break;
	}
	explicit_bzero(text, sizeof text);
	input->changed |= changed;
	return action;
}

static bool edits(enum key_action action) {
	return action == KEY_TEXT || action == KEY_BACKSPACE ||
	       action == KEY_CLEAR;
}

// keys typed while an attempt is checked are dropped, not queued
void input_press(struct input *input, struct keyboard *keyboard, uint32_t key) {
	if (input->auth->busy) {
		return;
	}
	// a lock unlocked another way never reads the Compose file
	if (!input->compose_loaded) {
		load_compose(input);
	}
	// a modifier leaves the held key repeating
	if (edits(handle(input, keyboard, key, false))) {
		start_repeat(input, keyboard, key);
	}
}

void input_release(
	struct input *input, const struct keyboard *keyboard, uint32_t key) {
	if (input->repeat_keyboard == keyboard && input->repeat_key == key) {
		stop_repeat(input);
	}
}

void input_forget(struct input *input, const struct keyboard *keyboard) {
	if (input->repeat_keyboard == keyboard) {
		stop_repeat(input);
	}
}

void input_repeat(struct input *input) {
	uint64_t expirations = 0;
	if (read(input->repeat_fd, &expirations, sizeof expirations) !=
			(ssize_t)sizeof expirations ||
		!input->repeat_keyboard) {
		return;
	}
	if (!edits(handle(
		    input, input->repeat_keyboard, input->repeat_key, true))) {
		stop_repeat(input);
	}
}

// Shift and the like change nothing on screen, so they cost no commit
void input_status(struct input *input, struct xkb_state *state) {
	struct input_status status;
	status.caps_lock =
		key_status(state, status.layout, sizeof status.layout);
	if (memcmp(&status, &input->status, sizeof status) != 0) {
		input->status = status;
		input->changed = true;
	}
}

void input_toggle_reveal(struct input *input) {
	input->revealed = !input->revealed;
	input->changed = true;
}
