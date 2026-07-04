#ifndef LOCKY_SEAT_H
#define LOCKY_SEAT_H

#include <stdbool.h>
#include <stdint.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

struct locky_state;

// Keymap and modifier state for the seat's keyboard
struct locky_xkb {
	struct xkb_context *context;
	struct xkb_keymap *keymap;
	struct xkb_state *state;
	bool control;
	bool caps_lock;
};

struct locky_seat {
	struct locky_state *state;
	struct wl_seat *wl_seat;
	struct wl_keyboard *keyboard;
	uint32_t global_name;
	struct wl_list link;
};

bool locky_xkb_init(struct locky_xkb *xkb);
void locky_xkb_finish(struct locky_xkb *xkb);

void locky_seat_create(struct locky_state *state, struct wl_registry *registry,
	uint32_t global_name, uint32_t version);
void locky_seat_destroy(struct locky_seat *seat);

#endif
