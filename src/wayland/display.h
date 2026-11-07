#ifndef SWEETLOCK_WAYLAND_DISPLAY_H
#define SWEETLOCK_WAYLAND_DISPLAY_H

#include <stdbool.h>
#include <time.h>
#include <wayland-util.h>

#include "config.h"
#include "ui/ui.h"
#include "wayland/lock.h"

struct input;
struct wl_surface;

struct display {
	struct wl_display *wl;
	struct wl_registry *registry;
	struct wl_compositor *compositor;
	struct wl_subcompositor *subcompositor;
	struct wl_shm *shm;
	struct wp_viewporter *viewporter;
	struct wp_fractional_scale_manager_v1 *fractional_scale;
	struct wp_cursor_shape_manager_v1 *cursor_shape;
	struct ext_session_lock_manager_v1 *lock_manager;
	struct wl_buffer *solid;
	struct lock lock;
	struct wl_list outputs;
	struct wl_list seats;
	// generated backgrounds, one per physical size in use
	struct wl_list backdrops;
	// a failed background is logged for the first output only
	bool background_warned;
	// every seat's keyboard types into it
	struct input *input;
	bool failed;
};

bool display_connect(struct display *display, const struct config *config,
	struct input *input);

// requests the lock and a lock surface for every output
bool display_lock(struct display *display, const struct timespec *start);

// starts and shows every mapped output's generated background
void display_show_backgrounds(
	struct display *display, const struct ui_content *content);

// draws every mapped output's UI that changed; true when any committed
bool display_render(struct display *display, const struct ui_content *content);

void display_mark_dirty(struct display *display, enum ui_part part);

// whether a point on a surface is on some output's reveal toggle
bool display_hits_eye(const struct display *display,
	const struct wl_surface *surface, double x, double y);

// logs why the connection ended, including any protocol error
void display_log_lost(struct display *display);

void display_disconnect(struct display *display);

#endif
