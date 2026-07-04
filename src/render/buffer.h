#ifndef LOCKY_BUFFER_H
#define LOCKY_BUFFER_H

#include <cairo.h>
#include <stdbool.h>
#include <stdint.h>
#include <wayland-client.h>

// A wl_shm buffer with a Cairo context bound to its mapped memory
struct locky_buffer {
	struct wl_buffer *wl_buffer;
	void *data;
	size_t size;
	int32_t width, height;
	cairo_surface_t *cairo_surface;
	cairo_t *cairo;
	bool busy; // attached and not yet released by the compositor
};

struct locky_buffer *locky_pool_get_buffer(struct locky_buffer pool[static 2],
	struct wl_shm *shm, int32_t width, int32_t height);

// Destroy both buffers in a pool
void locky_pool_destroy(struct locky_buffer pool[static 2]);

#endif
