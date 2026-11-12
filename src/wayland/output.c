#include "wayland/output.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-client.h>

#include "config/config.h"
#include "ext-session-lock-v1-client-protocol.h"
#include "fractional-scale-v1-client-protocol.h"
#include "log.h"
#include "viewporter-client-protocol.h"
#include "wayland/backdrop.h"
#include "wayland/buffer.h"
#include "wayland/display.h"
#include "wayland/panel.h"
#include "wayland/power.h"
#include "wayland/scale.h"

static uint32_t effective_scale(const struct output *output) {
	if (output->fractional_scale) {
		return output->fractional_scale;
	}
	return (uint32_t)output->integer_scale * SCALE_DENOMINATOR;
}

static void set_scale(
	struct output *output, uint32_t fractional, int32_t integer) {
	uint32_t old = effective_scale(output);
	output->fractional_scale = fractional;
	output->integer_scale = integer;
	uint32_t scale = effective_scale(output);
	if (scale != old) {
		// before the UI exists the integer and fractional events race
		log_at(output->ui ? LOG_INFO : LOG_DEBUG,
			"output %s scale is now %.3f", output->name,
			scale_factor(scale));
		output->relayout = true;
		output->ui_failed = false;
	}
}

static void output_geometry(void *data, struct wl_output *wl, int32_t x,
	int32_t y, int32_t physical_width, int32_t physical_height,
	int32_t subpixel, const char *make, const char *model,
	int32_t transform) {
	(void)data;
	(void)wl;
	(void)x;
	(void)y;
	(void)physical_width;
	(void)physical_height;
	(void)subpixel;
	(void)make;
	(void)model;
	(void)transform;
}

static void output_mode(void *data, struct wl_output *wl, uint32_t flags,
	int32_t width, int32_t height, int32_t refresh) {
	(void)data;
	(void)wl;
	(void)flags;
	(void)width;
	(void)height;
	(void)refresh;
}

static void output_done(void *data, struct wl_output *wl) {
	(void)wl;
	struct output *output = data;
	set_scale(output, output->fractional_scale,
		output->pending_integer_scale);
}

static void output_scale(void *data, struct wl_output *wl, int32_t factor) {
	(void)wl;
	struct output *output = data;
	if (factor <= 0 || factor > SCALE_MAX / SCALE_DENOMINATOR) {
		log_warn("ignoring scale %d for output %s", factor,
			output->name);
		return;
	}
	output->pending_integer_scale = factor;
}

// ui.output is matched here, against the name before it is cut or cleaned
static void output_name(void *data, struct wl_output *wl, const char *name) {
	(void)wl;
	struct output *output = data;
	const char *wanted = output->display->ui_output;
	output->ui_chosen = wanted[0] && strcmp(name, wanted) == 0;
	snprintf(output->name, sizeof output->name, "%s", name);
	for (char *c = output->name; *c; c++) {
		if ((unsigned char)*c < 0x20 || *c == 0x7f) {
			*c = '?';
		}
	}
	if (output->ui_chosen) {
		log_info("only output %s shows the UI (ui.output)",
			output->name);
	}
}

static void output_description(
	void *data, struct wl_output *wl, const char *description) {
	(void)data;
	(void)wl;
	(void)description;
}

static const struct wl_output_listener output_listener = {
	.geometry = output_geometry,
	.mode = output_mode,
	.done = output_done,
	.scale = output_scale,
	.name = output_name,
	.description = output_description,
};

static void release(struct wl_output *wl) {
	if (wl_output_get_version(wl) >= WL_OUTPUT_RELEASE_SINCE_VERSION) {
		wl_output_release(wl);
	} else {
		wl_output_destroy(wl);
	}
}

struct output *output_create(
	struct display *display, struct wl_output *wl, uint32_t global) {
	struct output *output = calloc(1, sizeof *output);
	if (!output) {
		log_error("out of memory allocating an output");
		release(wl);
		return NULL;
	}
	*output = (struct output){
		.display = display,
		.wl = wl,
		.global = global,
		.integer_scale = 1,
		.pending_integer_scale = 1,
	};
	snprintf(output->name, sizeof output->name, "wl_output@%u",
		wl_proxy_get_id((struct wl_proxy *)wl));
	wl_output_add_listener(wl, &output_listener, output);
	return output;
}

static void set_opaque(struct output *output, int32_t width, int32_t height) {
	struct wl_region *region =
		wl_compositor_create_region(output->display->compositor);
	// the opaque region is only a hint, so going without one is safe
	if (region) {
		wl_region_add(region, 0, 0, width, height);
		wl_surface_set_opaque_region(output->surface, region);
		wl_region_destroy(region);
	}
}

static void lock_surface_configure(void *data,
	struct ext_session_lock_surface_v1 *lock_surface, uint32_t serial,
	uint32_t width, uint32_t height) {
	struct output *output = data;
	if (width == 0 || height == 0 || width > INT32_MAX ||
		height > INT32_MAX) {
		log_warn("ignoring a %ux%u configure for output %s", width,
			height, output->name);
		return;
	}
	int32_t w = (int32_t)width;
	int32_t h = (int32_t)height;
	ext_session_lock_surface_v1_ack_configure(lock_surface, serial);
	wp_viewport_set_destination(output->viewport, w, h);
	set_opaque(output, w, h);
	if (!output->mapped) {
		wl_surface_attach(
			output->surface, output->display->solid, 0, 0);
		wl_surface_damage(output->surface, 0, 0, w, h);
	}
	wl_surface_commit(output->surface);
	log_info("%s output %s at %ux%u",
		output->mapped ? "resized" : "covered", output->name, width,
		height);
	output->mapped = true;
	output->width = w;
	output->height = h;
	output->relayout = true;
	output->ui_failed = false;
}

static const struct ext_session_lock_surface_v1_listener lock_surface_listener =
	{
		.configure = lock_surface_configure,
};

static void preferred_scale(
	void *data, struct wp_fractional_scale_v1 *fractional, uint32_t scale) {
	(void)fractional;
	struct output *output = data;
	if (!scale_valid(scale)) {
		log_warn("ignoring scale %u/%d for output %s", scale,
			SCALE_DENOMINATOR, output->name);
		return;
	}
	set_scale(output, scale, output->integer_scale);
}

static const struct wp_fractional_scale_v1_listener fractional_listener = {
	.preferred_scale = preferred_scale,
};

// the first frame is a 1x1 buffer, so the scale waits for the UI and the
// generated background
static bool attach_scale(struct output *output) {
	struct display *display = output->display;
	if (!display->fractional_scale) {
		return true;
	}
	output->fractional =
		wp_fractional_scale_manager_v1_get_fractional_scale(
			display->fractional_scale, output->surface);
	if (!output->fractional) {
		return false;
	}
	wp_fractional_scale_v1_add_listener(
		output->fractional, &fractional_listener, output);
	return true;
}

bool output_lock(struct output *output) {
	struct display *display = output->display;
	output->surface = wl_compositor_create_surface(display->compositor);
	if (output->surface) {
		output->viewport = wp_viewporter_get_viewport(
			display->viewporter, output->surface);
	}
	if (output->viewport && attach_scale(output)) {
		output->lock_surface = ext_session_lock_v1_get_lock_surface(
			display->lock.wl, output->surface, output->wl);
	}
	if (!output->lock_surface) {
		log_error("failed to create a lock surface for output %s",
			output->name);
		return false;
	}
	ext_session_lock_surface_v1_add_listener(
		output->lock_surface, &lock_surface_listener, output);
	return true;
}

// until a new size's background is ready, the old one is stretched over it
void output_show_background(
	struct output *output, const struct ui_content *content) {
	if (!output->mapped ||
		content->config->background.mode == BACKGROUND_SOLID ||
		output->width > SCALE_SIDE_MAX ||
		output->height > SCALE_SIDE_MAX) {
		return;
	}
	uint32_t scale = effective_scale(output);
	int32_t width = scale_physical(scale, output->width);
	int32_t height = scale_physical(scale, output->height);
	uint32_t shared = background_scale(content->config, scale);
	struct backdrop *wanted = output->backdrop;
	if (!wanted || wanted->width != width || wanted->height != height ||
		wanted->scale != shared) {
		backdrop_put(wanted);
		output->backdrop = backdrop_get(output->display,
			content->config, content->worker, width, height, scale);
		wanted = output->backdrop;
	}
	if (!wanted || !wanted->ready || output->shown == wanted) {
		return;
	}
	wl_surface_attach(output->surface, wanted->buffer->wl, 0, 0);
	wl_surface_damage(output->surface, 0, 0, output->width, output->height);
	wl_surface_commit(output->surface);
	wanted->refs++;
	backdrop_put(output->shown);
	output->shown = wanted;
}

static void drop_ui(struct output *output) {
	for (int i = 0; i < UI_PARTS; i++) {
		panel_destroy(output->panels[i]);
		output->panels[i] = NULL;
	}
	ui_destroy(output->ui);
	output->ui = NULL;
}

static bool create_ui(struct output *output, const struct ui_content *content) {
	output->ui = ui_create(content, effective_scale(output));
	if (!output->ui) {
		return false;
	}
	for (int i = 0; i < UI_PARTS; i++) {
		if (!ui_shows(content, (enum ui_part)i)) {
			continue;
		}
		output->panels[i] = panel_create(
			output->display, output->surface, (enum ui_part)i);
		if (!output->panels[i]) {
			return false;
		}
	}
	output->relayout = true;
	return true;
}

// a new panel position takes effect with the lock surface's next commit
static bool place(struct output *output) {
	uint32_t scale = effective_scale(output);
	ui_set_scale(output->ui, scale);
	bool moved = false;
	for (int i = 0; i < UI_PARTS; i++) {
		struct ui_rect rect;
		if (output->panels[i] &&
			(!ui_place(output->ui->content, (enum ui_part)i,
				 output->width, output->height, scale, &rect) ||
				!panel_place(output->panels[i], &rect, scale,
					&moved))) {
			return false;
		}
	}
	if (moved) {
		wl_surface_commit(output->surface);
	}
	output->relayout = false;
	return true;
}

static bool render(
	struct output *output, const struct ui_content *content, bool *ok) {
	*ok = (output->ui || create_ui(output, content)) &&
	      (!output->relayout || place(output));
	bool committed = false;
	for (int i = 0; *ok && i < UI_PARTS; i++) {
		if (!output->panels[i]) {
			continue;
		}
		enum panel_result result =
			panel_render(output->panels[i], output->ui);
		*ok = result != PANEL_FAILED;
		committed |= result == PANEL_COMMITTED;
	}
	return committed;
}

// a dark output commits nothing; what changed meanwhile is drawn on wake
bool output_render(struct output *output, const struct ui_content *content,
	bool shows_ui) {
	if (output->power_off) {
		return false;
	}
	output_show_background(output, content);
	if (!shows_ui && output->ui) {
		drop_ui(output);
	}
	if (!shows_ui || !output->mapped || output->ui_failed) {
		return false;
	}
	bool ok = true;
	bool committed = render(output, content, &ok);
	if (!ok) {
		// a new size or scale tries again
		log_error("dropping the UI on output %s; it stays locked",
			output->name);
		drop_ui(output);
		output->ui_failed = true;
	}
	return committed;
}

void output_mark_dirty(struct output *output, enum ui_part part) {
	if (output->panels[part]) {
		output->panels[part]->dirty = true;
	}
}

// pointer coordinates on the login part are its logical pixels
bool output_hits_eye(const struct output *output,
	const struct wl_surface *surface, double x, double y) {
	const struct panel *panel = output->panels[UI_LOGIN];
	return panel && output->ui && panel->surface == surface &&
	       ui_hits_eye(output->ui, &panel->rect, x, y);
}

// the UI goes first, then the role object before its wl_surface, and both
// before the output
void output_destroy(struct output *output) {
	if (!output) {
		return;
	}
	drop_ui(output);
	if (output->fractional) {
		wp_fractional_scale_v1_destroy(output->fractional);
	}
	if (output->lock_surface) {
		ext_session_lock_surface_v1_destroy(output->lock_surface);
	}
	if (output->viewport) {
		wp_viewport_destroy(output->viewport);
	}
	if (output->surface) {
		wl_surface_destroy(output->surface);
	}
	backdrop_put(output->backdrop);
	backdrop_put(output->shown);
	power_detach(output);
	release(output->wl);
	free(output);
}
