#ifndef SWEETLOCK_WAYLAND_BUFFER_H
#define SWEETLOCK_WAYLAND_BUFFER_H

#include <cairo.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "config/config.h"

struct wl_shm;

struct buffer {
	struct wl_buffer *wl;
	// draws straight into the shared memory
	cairo_surface_t *cairo;
	void *data;
	size_t size;
	int32_t width;
	int32_t height;
	bool busy;
};

struct wl_buffer *buffer_solid(struct wl_shm *shm, struct color color);

struct buffer *buffer_create(
	struct wl_shm *shm, int32_t width, int32_t height, bool opaque);

void buffer_destroy(struct buffer *buffer);

#endif
