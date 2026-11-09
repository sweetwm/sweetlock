#include "wayland/panel.h"

#include <stdlib.h>
#include <wayland-client.h>

#include "log.h"
#include "viewporter-client-protocol.h"
#include "wayland/buffer.h"
#include "wayland/display.h"
#include "wayland/scale.h"

struct panel *panel_create(
	struct display *display, struct wl_surface *parent, enum ui_part part) {
	struct panel *panel = calloc(1, sizeof *panel);
	if (!panel) {
		log_error("out of memory allocating a UI surface");
		return NULL;
	}
	*panel = (struct panel){.display = display, .part = part};
	panel->surface = wl_compositor_create_surface(display->compositor);
	if (panel->surface) {
		panel->subsurface = wl_subcompositor_get_subsurface(
			display->subcompositor, panel->surface, parent);
		panel->viewport = wp_viewporter_get_viewport(
			display->viewporter, panel->surface);
	}
	if (!panel->subsurface || !panel->viewport) {
		log_error("failed to create a UI surface");
		panel_destroy(panel);
		return NULL;
	}
	// typing and clock ticks redraw it without a commit of the lock
	// surface
	wl_subsurface_set_desync(panel->subsurface);
	return panel;
}

static void release_buffers(struct panel *panel) {
	for (int i = 0; i < PANEL_BUFFERS; i++) {
		buffer_destroy(panel->buffers[i]);
		panel->buffers[i] = NULL;
	}
}

// all or nothing, so a failure keeps the old buffers
static bool replace_buffers(
	struct panel *panel, int32_t width, int32_t height) {
	struct buffer *fresh[PANEL_BUFFERS] = {0};
	for (int i = 0; i < PANEL_BUFFERS; i++) {
		fresh[i] = buffer_create(
			panel->display->shm, width, height, false);
		if (!fresh[i]) {
			for (int j = 0; j < i; j++) {
				buffer_destroy(fresh[j]);
			}
			return false;
		}
	}
	release_buffers(panel);
	for (int i = 0; i < PANEL_BUFFERS; i++) {
		panel->buffers[i] = fresh[i];
	}
	panel->next_buffer = 0;
	return true;
}

bool panel_place(struct panel *panel, const struct ui_rect *rect,
	uint32_t scale, bool *moved) {
	if (rect->width != panel->rect.width ||
		rect->height != panel->rect.height || scale != panel->scale) {
		if (!replace_buffers(panel, scale_physical(scale, rect->width),
			    scale_physical(scale, rect->height))) {
			return false;
		}
		panel->dirty = true;
	}
	if (rect->x != panel->rect.x || rect->y != panel->rect.y ||
		panel->scale == 0) {
		wl_subsurface_set_position(panel->subsurface, rect->x, rect->y);
		*moved = true;
	}
	panel->rect = *rect;
	panel->scale = scale;
	return true;
}

void panel_mark_dirty(struct panel *panel) {
	panel->dirty = true;
}

static void frame_done(void *data, struct wl_callback *callback, uint32_t ms) {
	(void)ms;
	struct panel *panel = data;
	wl_callback_destroy(callback);
	panel->frame = NULL;
}

static const struct wl_callback_listener frame_listener = {
	.done = frame_done,
};

static struct buffer *free_buffer(struct panel *panel) {
	for (int i = 0; i < PANEL_BUFFERS; i++) {
		int index = (panel->next_buffer + i) % PANEL_BUFFERS;
		struct buffer *buffer = panel->buffers[index];
		if (buffer && !buffer->busy) {
			panel->next_buffer = (index + 1) % PANEL_BUFFERS;
			return buffer;
		}
	}
	return NULL;
}

static bool draw(
	const struct panel *panel, struct buffer *buffer, struct ui *ui) {
	cairo_t *cr = cairo_create(buffer->cairo);
	cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
	cairo_paint(cr);
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	bool ok = ui_draw(ui, panel->part, cr, &panel->rect);
	cairo_destroy(cr);
	cairo_surface_flush(buffer->cairo);
	return ok;
}

// the viewport destination goes out in the same commit as its buffer
static void attach(struct panel *panel, struct buffer *buffer) {
	wp_viewport_set_destination(
		panel->viewport, panel->rect.width, panel->rect.height);
	wl_surface_attach(panel->surface, buffer->wl, 0, 0);
	if (wl_surface_get_version(panel->surface) >=
		WL_SURFACE_DAMAGE_BUFFER_SINCE_VERSION) {
		wl_surface_damage_buffer(
			panel->surface, 0, 0, buffer->width, buffer->height);
	} else {
		wl_surface_damage(panel->surface, 0, 0, INT32_MAX, INT32_MAX);
	}
	buffer->busy = true;
}

enum panel_result panel_render(struct panel *panel, struct ui *ui) {
	if (!panel->dirty || panel->frame) {
		return PANEL_IDLE;
	}
	// a release wakes the loop, which comes back here
	struct buffer *buffer = free_buffer(panel);
	if (!buffer) {
		return PANEL_IDLE;
	}
	if (!draw(panel, buffer, ui)) {
		return PANEL_FAILED;
	}
	panel->frame = wl_surface_frame(panel->surface);
	if (!panel->frame) {
		log_error("failed to request a frame callback");
		return PANEL_FAILED;
	}
	wl_callback_add_listener(panel->frame, &frame_listener, panel);
	attach(panel, buffer);
	wl_surface_commit(panel->surface);
	panel->dirty = false;
	return PANEL_COMMITTED;
}

void panel_destroy(struct panel *panel) {
	if (!panel) {
		return;
	}
	if (panel->frame) {
		wl_callback_destroy(panel->frame);
	}
	if (panel->subsurface) {
		wl_subsurface_destroy(panel->subsurface);
	}
	if (panel->viewport) {
		wp_viewport_destroy(panel->viewport);
	}
	if (panel->surface) {
		wl_surface_destroy(panel->surface);
	}
	release_buffers(panel);
	free(panel);
}
