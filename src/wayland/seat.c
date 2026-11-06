#include "wayland/seat.h"

#include <stdbool.h>
#include <stdlib.h>
#include <wayland-client.h>

#include "input/keyboard.h"
#include "log.h"
#include "wayland/display.h"

// without its keyboard the session cannot be unlocked, so a failure here
// ends the locker and leaves the restart to the compositor
static void seat_capabilities(
	void *data, struct wl_seat *wl, uint32_t capabilities) {
	struct seat *seat = data;
	bool keyboard = capabilities & WL_SEAT_CAPABILITY_KEYBOARD;
	if (keyboard && !seat->keyboard) {
		struct wl_keyboard *proxy = wl_seat_get_keyboard(wl);
		seat->keyboard =
			proxy ? keyboard_create(seat->display->input, proxy)
			      : NULL;
		if (!seat->keyboard) {
			log_error("failed to get a seat's keyboard");
			seat->display->failed = true;
		}
	} else if (!keyboard && seat->keyboard) {
		keyboard_destroy(seat->keyboard);
		seat->keyboard = NULL;
	}
}

static void seat_name(void *data, struct wl_seat *wl, const char *name) {
	(void)data;
	(void)wl;
	(void)name;
}

static const struct wl_seat_listener seat_listener = {
	.capabilities = seat_capabilities,
	.name = seat_name,
};

static void release(struct wl_seat *wl) {
	if (wl_seat_get_version(wl) >= WL_SEAT_RELEASE_SINCE_VERSION) {
		wl_seat_release(wl);
	} else {
		wl_seat_destroy(wl);
	}
}

struct seat *seat_create(
	struct display *display, struct wl_seat *wl, uint32_t global) {
	struct seat *seat = calloc(1, sizeof *seat);
	if (!seat) {
		log_error("out of memory allocating a seat");
		release(wl);
		return NULL;
	}
	*seat = (struct seat){
		.display = display,
		.wl = wl,
		.global = global,
	};
	wl_seat_add_listener(wl, &seat_listener, seat);
	return seat;
}

void seat_destroy(struct seat *seat) {
	if (!seat) {
		return;
	}
	keyboard_destroy(seat->keyboard);
	release(seat->wl);
	free(seat);
}
