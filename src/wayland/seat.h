#ifndef SWEETLOCK_WAYLAND_SEAT_H
#define SWEETLOCK_WAYLAND_SEAT_H

#include <stdint.h>
#include <wayland-util.h>

struct display;
struct keyboard;
struct pointer;
struct wl_seat;

struct seat {
	struct wl_list link;
	struct display *display;
	struct wl_seat *wl;
	// the registry name, matched against global_remove
	uint32_t global;
	struct keyboard *keyboard;
	struct pointer *pointer;
};

// takes ownership of wl, released on failure
struct seat *seat_create(
	struct display *display, struct wl_seat *wl, uint32_t global);

void seat_destroy(struct seat *seat);

#endif
