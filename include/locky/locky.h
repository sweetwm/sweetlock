#ifndef LOCKY_LOCKY_H
#define LOCKY_LOCKY_H

#include "locky/buffer.h"
#include "locky/password.h"
#include "locky/seat.h"
#include <stdbool.h>
#include <stdint.h>
#include <wayland-client.h>

struct locky_state {
	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_compositor *compositor;
	struct wl_shm *shm;
	struct ext_session_lock_manager_v1 *lock_manager;
	struct ext_session_lock_v1 *lock;
	struct wl_list outputs; // locky_output.link
	struct wl_list seats;	// locky_seat.link

	struct locky_xkb xkb;
	struct locky_password password;

	uint32_t background; // 0x00RRGGBB, opaque
	bool locked;
	bool finished;
	bool running;
};

struct locky_output {
	struct locky_state *state;
	struct wl_output *wl_output;
	uint32_t global_name;

	struct wl_surface *surface;
	struct ext_session_lock_surface_v1 *lock_surface;

	uint32_t width, height; // surface-local size from the last configure
	int32_t scale;

	struct locky_buffer buffers[2]; // double-buffered pool for this output

	struct wl_list link;
};

#endif
