#include "input/keyboard.h"

#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon-compose.h>
#include <xkbcommon/xkbcommon.h>

#include "input/input.h"
#include "log.h"

enum {
	// real keymaps are well under 1 MiB; this only bounds a hostile one
	KEYMAP_MAX = 4 * 1024 * 1024,
	// wl_keyboard sends evdev codes, xkb numbers them 8 higher
	EVDEV_OFFSET = 8,
	REPEAT_RATE_MAX = 1000,
	// used until the compositor sends its own timing
	REPEAT_RATE_DEFAULT = 25,
	REPEAT_DELAY_DEFAULT = 600,
};

struct keyboard {
	struct input *input;
	struct wl_keyboard *wl;
	struct xkb_keymap *keymap;
	struct xkb_state *state;
	// made on the first key once the input has a Compose table
	struct xkb_compose_state *compose;
	int32_t repeat_rate;
	int32_t repeat_delay;
};

static struct xkb_keymap *compile(
	struct xkb_context *xkb, int fd, uint32_t size) {
	// reading a mapped page past the end of the file raises SIGBUS
	struct stat file;
	if (fstat(fd, &file) != 0 || file.st_size < (off_t)size) {
		return NULL;
	}
	char *text = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (text == MAP_FAILED) {
		log_warn("failed to map the keymap: %m");
		return NULL;
	}
	struct xkb_keymap *keymap = xkb_keymap_new_from_buffer(xkb, text, size,
		XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
	munmap(text, size);
	return keymap;
}

// a keymap that cannot be used leaves the previous one in place
static void keyboard_keymap(void *data, struct wl_keyboard *wl, uint32_t format,
	int32_t fd, uint32_t size) {
	(void)wl;
	struct keyboard *keyboard = data;
	struct xkb_keymap *keymap = NULL;
	if (format == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 && size > 0 &&
		size <= KEYMAP_MAX) {
		keymap = compile(keyboard->input->xkb, fd, size);
	}
	close(fd);
	struct xkb_state *state = keymap ? xkb_state_new(keymap) : NULL;
	if (!state) {
		log_warn("ignoring an unusable keymap (format %u, %u bytes)",
			format, size);
		xkb_keymap_unref(keymap);
		return;
	}
	input_forget(keyboard->input, keyboard);
	xkb_state_unref(keyboard->state);
	xkb_keymap_unref(keyboard->keymap);
	keyboard->keymap = keymap;
	keyboard->state = state;
	if (keyboard->compose) {
		xkb_compose_state_reset(keyboard->compose);
	}
}

// keys already held when focus arrives are not typed
static void keyboard_enter(void *data, struct wl_keyboard *wl, uint32_t serial,
	struct wl_surface *surface, struct wl_array *keys) {
	(void)data;
	(void)wl;
	(void)serial;
	(void)surface;
	(void)keys;
}

static void keyboard_leave(void *data, struct wl_keyboard *wl, uint32_t serial,
	struct wl_surface *surface) {
	(void)wl;
	(void)serial;
	(void)surface;
	struct keyboard *keyboard = data;
	input_forget(keyboard->input, keyboard);
	if (keyboard->compose) {
		xkb_compose_state_reset(keyboard->compose);
	}
}

static void keyboard_key(void *data, struct wl_keyboard *wl, uint32_t serial,
	uint32_t time, uint32_t key, uint32_t state) {
	(void)wl;
	(void)serial;
	(void)time;
	struct keyboard *keyboard = data;
	if (!keyboard->state || key > UINT32_MAX - EVDEV_OFFSET) {
		return;
	}
	if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		input_status(keyboard->input, keyboard->state);
		input_press(keyboard->input, keyboard, key);
	} else if (state == WL_KEYBOARD_KEY_STATE_RELEASED) {
		input_release(keyboard->input, keyboard, key);
	}
}

static void keyboard_modifiers(void *data, struct wl_keyboard *wl,
	uint32_t serial, uint32_t depressed, uint32_t latched, uint32_t locked,
	uint32_t group) {
	(void)wl;
	(void)serial;
	struct keyboard *keyboard = data;
	if (keyboard->state) {
		xkb_state_update_mask(keyboard->state, depressed, latched,
			locked, 0, 0, group);
		input_status(keyboard->input, keyboard->state);
	}
}

static void keyboard_repeat_info(
	void *data, struct wl_keyboard *wl, int32_t rate, int32_t delay) {
	(void)wl;
	struct keyboard *keyboard = data;
	input_forget(keyboard->input, keyboard);
	keyboard->repeat_rate = rate < 0 ? 0 : rate;
	if (keyboard->repeat_rate > REPEAT_RATE_MAX) {
		keyboard->repeat_rate = REPEAT_RATE_MAX;
	}
	keyboard->repeat_delay = delay < 0 ? 0 : delay;
}

static const struct wl_keyboard_listener keyboard_listener = {
	.keymap = keyboard_keymap,
	.enter = keyboard_enter,
	.leave = keyboard_leave,
	.key = keyboard_key,
	.modifiers = keyboard_modifiers,
	.repeat_info = keyboard_repeat_info,
};

static void release(struct wl_keyboard *wl) {
	if (wl_keyboard_get_version(wl) >= WL_KEYBOARD_RELEASE_SINCE_VERSION) {
		wl_keyboard_release(wl);
	} else {
		wl_keyboard_destroy(wl);
	}
}

struct keyboard *keyboard_create(struct input *input, struct wl_keyboard *wl) {
	struct keyboard *keyboard = calloc(1, sizeof *keyboard);
	if (!keyboard) {
		log_error("out of memory allocating a keyboard");
		release(wl);
		return NULL;
	}
	*keyboard = (struct keyboard){
		.input = input,
		.wl = wl,
		.repeat_rate = REPEAT_RATE_DEFAULT,
		.repeat_delay = REPEAT_DELAY_DEFAULT,
	};
	wl_keyboard_add_listener(wl, &keyboard_listener, keyboard);
	return keyboard;
}

void keyboard_destroy(struct keyboard *keyboard) {
	if (!keyboard) {
		return;
	}
	input_forget(keyboard->input, keyboard);
	release(keyboard->wl);
	xkb_compose_state_unref(keyboard->compose);
	xkb_state_unref(keyboard->state);
	xkb_keymap_unref(keyboard->keymap);
	free(keyboard);
}

enum key_action keyboard_translate(
	struct keyboard *keyboard, uint32_t key, char *text, size_t size) {
	struct xkb_compose_table *table = keyboard->input->compose;
	if (table && !keyboard->compose) {
		keyboard->compose = xkb_compose_state_new(
			table, XKB_COMPOSE_STATE_NO_FLAGS);
	}
	return key_translate(keyboard->state, keyboard->compose,
		key + EVDEV_OFFSET, text, size);
}

bool keyboard_is_escape(const struct keyboard *keyboard, uint32_t key) {
	return xkb_state_key_get_one_sym(keyboard->state, key + EVDEV_OFFSET) ==
	       XKB_KEY_Escape;
}

bool keyboard_repeats(const struct keyboard *keyboard, uint32_t key,
	int32_t *rate, int32_t *delay) {
	*rate = keyboard->repeat_rate;
	*delay = keyboard->repeat_delay;
	return keyboard->repeat_rate > 0 &&
	       xkb_keymap_key_repeats(keyboard->keymap, key + EVDEV_OFFSET);
}
