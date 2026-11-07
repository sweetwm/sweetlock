#include "input/input.h"

#include <string.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include <xkbcommon/xkbcommon.h>

#include "auth/auth.h"
#include "input/keyboard.h"
#include "input/password.h"
#include "log.h"

bool input_init(struct input *input, struct auth *auth) {
	*input = (struct input){.auth = auth, .repeat_fd = -1};
	// compositor keymaps arrive compiled, so xkb never reads its include
	// paths
	input->xkb = xkb_context_new(XKB_CONTEXT_NO_DEFAULT_INCLUDES);
	if (!input->xkb) {
		log_error("failed to create the xkb context");
		return false;
	}
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
	xkb_context_unref(input->xkb);
	*input = (struct input){.repeat_fd = -1};
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

void input_toggle_reveal(struct input *input) {
	input->revealed = !input->revealed;
	input->changed = true;
}
