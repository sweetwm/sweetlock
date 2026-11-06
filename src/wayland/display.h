#ifndef SWEETLOCK_WAYLAND_DISPLAY_H
#define SWEETLOCK_WAYLAND_DISPLAY_H

#include <stdbool.h>
#include <time.h>
#include <wayland-util.h>

#include "config.h"
#include "ui/ui.h"
#include "wayland/lock.h"

struct input;

struct display {
	struct wl_display *wl;
	struct wl_registry *registry;
	struct wl_compositor *compositor;
	struct wl_subcompositor *subcompositor;
	struct wl_shm *shm;
	struct wp_viewporter *viewporter;
	// optional; wl_output.scale is the fallback
	struct wp_fractional_scale_manager_v1 *fractional_scale;
	struct ext_session_lock_manager_v1 *lock_manager;
	struct wl_buffer *solid;
	struct lock lock;
	struct wl_list outputs;
	struct wl_list seats;
	// every seat's keyboard types into it
	struct input *input;
	bool failed;
};

bool display_connect(struct display *display, const struct config *config,
	struct input *input);

// requests the lock and a lock surface for every output
bool display_lock(struct display *display, const struct timespec *start);

// draws every mapped output's UI that changed; true when any committed
bool display_render(struct display *display, const struct ui_content *content);

void display_mark_dirty(struct display *display, enum ui_part part);

// logs why the connection ended, including any protocol error
void display_log_lost(struct display *display);

// never unlocks: a lock still held is dropped without a request
void display_disconnect(struct display *display);

#endif
