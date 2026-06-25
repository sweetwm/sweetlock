#include "locky/seat.h"
#include "locky/locky.h"
#include "locky/log.h"
#include "locky/password.h"
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

bool locky_xkb_init(struct locky_xkb *xkb) {
	xkb->context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	if (xkb->context == NULL) {
		locky_log(LOCKY_ERROR, "failed to create xkb context");
		return false;
	}
	return true;
}

void locky_xkb_finish(struct locky_xkb *xkb) {
	xkb_state_unref(xkb->state);
	xkb_keymap_unref(xkb->keymap);
	xkb_context_unref(xkb->context);
	xkb->state = NULL;
	xkb->keymap = NULL;
	xkb->context = NULL;
}

static void keyboard_keymap(void *data, struct wl_keyboard *keyboard,
	uint32_t format, int32_t fd, uint32_t size) {
	(void)keyboard;
	struct locky_seat *seat = data;
	struct locky_xkb *xkb = &seat->state->xkb;

	if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 ||
		xkb->context == NULL) {
		close(fd);
		return;
	}

	char *map = mmap(NULL, size - 1, PROT_READ, MAP_PRIVATE, fd, 0);
	if (map == MAP_FAILED) {
		close(fd);
		locky_log(LOCKY_ERROR, "failed to mmap keymap");
		return;
	}

	struct xkb_keymap *keymap =
		xkb_keymap_new_from_buffer(xkb->context, map, size - 1,
			XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
	munmap(map, size - 1);
	close(fd);
	if (keymap == NULL) {
		locky_log(LOCKY_ERROR, "failed to compile keymap");
		return;
	}

	struct xkb_state *xkb_state = xkb_state_new(keymap);
	if (xkb_state == NULL) {
		xkb_keymap_unref(keymap);
		locky_log(LOCKY_ERROR, "failed to create xkb state");
		return;
	}

	xkb_state_unref(xkb->state);
	xkb_keymap_unref(xkb->keymap);
	xkb->keymap = keymap;
	xkb->state = xkb_state;
}

static void keyboard_enter(void *data, struct wl_keyboard *keyboard,
	uint32_t serial, struct wl_surface *surface, struct wl_array *keys) {
	(void)data, (void)keyboard, (void)serial, (void)surface, (void)keys;
}

static void keyboard_leave(void *data, struct wl_keyboard *keyboard,
	uint32_t serial, struct wl_surface *surface) {
	(void)data, (void)keyboard, (void)serial, (void)surface;
}

static void keyboard_key(void *data, struct wl_keyboard *keyboard,
	uint32_t serial, uint32_t time, uint32_t key, uint32_t key_state) {
	(void)keyboard, (void)serial, (void)time;
	struct locky_seat *seat = data;
	struct locky_xkb *xkb = &seat->state->xkb;
	if (xkb->state == NULL || key_state != WL_KEYBOARD_KEY_STATE_PRESSED) {
		return;
	}

	// libxkbcommon keycodes are offset by 8 from the evdev codes
	xkb_keycode_t keycode = key + 8;
	xkb_keysym_t keysym = xkb_state_key_get_one_sym(xkb->state, keycode);
	uint32_t codepoint = xkb_state_key_get_utf32(xkb->state, keycode);
	locky_keyboard_handle_key(seat->state, keysym, codepoint);
}

static void keyboard_modifiers(void *data, struct wl_keyboard *keyboard,
	uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched,
	uint32_t mods_locked, uint32_t group) {
	(void)keyboard, (void)serial;
	struct locky_seat *seat = data;
	struct locky_xkb *xkb = &seat->state->xkb;
	if (xkb->state == NULL) {
		return;
	}
	xkb_state_update_mask(xkb->state, mods_depressed, mods_latched,
		mods_locked, 0, 0, group);
	xkb->control =
		xkb_state_mod_name_is_active(xkb->state, XKB_MOD_NAME_CTRL,
			XKB_STATE_MODS_DEPRESSED | XKB_STATE_MODS_LATCHED);
	xkb->caps_lock = xkb_state_mod_name_is_active(
		xkb->state, XKB_MOD_NAME_CAPS, XKB_STATE_MODS_LOCKED);
}

static void keyboard_repeat_info(
	void *data, struct wl_keyboard *keyboard, int32_t rate, int32_t delay) {
	(void)data, (void)keyboard, (void)rate, (void)delay;
	// TODO: key repeat needs a timer source in the event loop
}

static const struct wl_keyboard_listener keyboard_listener = {
	.keymap = keyboard_keymap,
	.enter = keyboard_enter,
	.leave = keyboard_leave,
	.key = keyboard_key,
	.modifiers = keyboard_modifiers,
	.repeat_info = keyboard_repeat_info,
};

static void release_keyboard(struct locky_seat *seat) {
	if (seat->keyboard == NULL) {
		return;
	}
	if (wl_keyboard_get_version(seat->keyboard) >=
		WL_KEYBOARD_RELEASE_SINCE_VERSION) {
		wl_keyboard_release(seat->keyboard);
	} else {
		wl_keyboard_destroy(seat->keyboard);
	}
	seat->keyboard = NULL;
}

static void seat_handle_capabilities(
	void *data, struct wl_seat *wl_seat, uint32_t capabilities) {
	struct locky_seat *seat = data;
	bool has_keyboard = (capabilities & WL_SEAT_CAPABILITY_KEYBOARD) != 0;

	if (has_keyboard && seat->keyboard == NULL) {
		seat->keyboard = wl_seat_get_keyboard(wl_seat);
		wl_keyboard_add_listener(
			seat->keyboard, &keyboard_listener, seat);
	} else if (!has_keyboard && seat->keyboard != NULL) {
		release_keyboard(seat);
	}
}

static void seat_handle_name(
	void *data, struct wl_seat *wl_seat, const char *name) {
	(void)data, (void)wl_seat, (void)name;
}

static const struct wl_seat_listener seat_listener = {
	.capabilities = seat_handle_capabilities,
	.name = seat_handle_name,
};

void locky_seat_create(struct locky_state *state, struct wl_registry *registry,
	uint32_t global_name, uint32_t version) {
	struct locky_seat *seat = calloc(1, sizeof(*seat));
	if (seat == NULL) {
		return;
	}
	seat->state = state;
	seat->global_name = global_name;

	uint32_t bind_version = version < 5 ? version : 5;
	seat->wl_seat = wl_registry_bind(
		registry, global_name, &wl_seat_interface, bind_version);
	wl_seat_add_listener(seat->wl_seat, &seat_listener, seat);
	wl_list_insert(&state->seats, &seat->link);
}

void locky_seat_destroy(struct locky_seat *seat) {
	wl_list_remove(&seat->link);
	release_keyboard(seat);
	if (wl_seat_get_version(seat->wl_seat) >=
		WL_SEAT_RELEASE_SINCE_VERSION) {
		wl_seat_release(seat->wl_seat);
	} else {
		wl_seat_destroy(seat->wl_seat);
	}
	free(seat);
}
