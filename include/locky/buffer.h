#ifndef LOCKY_BUFFER_H
#define LOCKY_BUFFER_H

#include <stdint.h>
#include <wayland-client.h>

struct wl_buffer *locky_buffer_create_solid(
	struct wl_shm *shm, int32_t width, int32_t height, uint32_t color);

#endif
