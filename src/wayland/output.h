#ifndef SWEETLOCK_WAYLAND_OUTPUT_H
#define SWEETLOCK_WAYLAND_OUTPUT_H

#include <stdbool.h>
#include <stdint.h>
#include <wayland-util.h>

struct display;
struct wl_output;

enum { OUTPUT_NAME_SIZE = 64 };

struct output {
	struct wl_list link;
	struct display *display;
	struct wl_output *wl;
	// the registry name, matched against global_remove
	uint32_t global;
	char name[OUTPUT_NAME_SIZE];
	struct wl_surface *surface;
	struct wp_viewport *viewport;
	struct ext_session_lock_surface_v1 *lock_surface;
	// a buffer is attached, so later configures only resize
	bool mapped;
};

// takes ownership of wl, released on failure
struct output *output_create(
	struct display *display, struct wl_output *wl, uint32_t global);

// creates this output's lock surface on the display's lock
bool output_lock(struct output *output);

void output_destroy(struct output *output);

#endif
