#ifndef SWEETLOCK_WAYLAND_DISPLAY_H
#define SWEETLOCK_WAYLAND_DISPLAY_H

#include <stdbool.h>
#include <time.h>
#include <wayland-util.h>

#include "config.h"
#include "wayland/lock.h"

struct display {
	struct wl_display *wl;
	struct wl_registry *registry;
	struct wl_compositor *compositor;
	struct wl_shm *shm;
	struct wp_viewporter *viewporter;
	struct ext_session_lock_manager_v1 *lock_manager;
	struct wl_buffer *solid;
	struct lock lock;
	struct wl_list outputs;
	bool failed;
};

bool display_connect(struct display *display, const struct config *config);

// requests the lock and a lock surface for every output
bool display_lock(struct display *display, const struct timespec *start);

// logs why the connection ended, including any protocol error
void display_log_lost(struct display *display);

// never unlocks: a lock still held is dropped without a request
void display_disconnect(struct display *display);

#endif
