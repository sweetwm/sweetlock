#ifndef SWEETLOCK_WAYLAND_BUFFER_H
#define SWEETLOCK_WAYLAND_BUFFER_H

#include "config.h"

struct wl_shm;

struct wl_buffer *buffer_solid(struct wl_shm *shm, struct color color);

#endif
