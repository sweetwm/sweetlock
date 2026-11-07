#ifndef SWEETLOCK_WAYLAND_PANEL_H
#define SWEETLOCK_WAYLAND_PANEL_H

#include <stdbool.h>
#include <stdint.h>

#include "ui/ui.h"

struct buffer;
struct display;
struct wl_surface;

enum { PANEL_BUFFERS = 2 };

enum panel_result {
	PANEL_IDLE,
	PANEL_COMMITTED,
	PANEL_FAILED,
};

// one part of the UI as a desynchronized subsurface of a lock surface
struct panel {
	struct display *display;
	enum ui_part part;
	struct wl_surface *surface;
	struct wl_subsurface *subsurface;
	struct wp_viewport *viewport;
	struct wl_callback *frame;
	struct buffer *buffers[PANEL_BUFFERS];
	int next_buffer;
	struct ui_rect rect;
	uint32_t scale;
	bool dirty;
};

struct panel *panel_create(
	struct display *display, struct wl_surface *parent, enum ui_part part);

bool panel_place(struct panel *panel, const struct ui_rect *rect,
	uint32_t scale, bool *moved);

void panel_mark_dirty(struct panel *panel);

// draws and commits when dirty, with no frame pending and a free buffer
enum panel_result panel_render(struct panel *panel, struct ui *ui);

// the frame callback, subsurface, viewport and surface go before the buffers
void panel_destroy(struct panel *panel);

#endif
