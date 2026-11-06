#include "wayland/output.h"

#include <stdio.h>
#include <stdlib.h>
#include <wayland-client.h>

#include "ext-session-lock-v1-client-protocol.h"
#include "log.h"
#include "viewporter-client-protocol.h"
#include "wayland/display.h"

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
	(void)data;
	(void)wl;
}

static void output_scale(void *data, struct wl_output *wl, int32_t factor) {
	(void)data;
	(void)wl;
	(void)factor;
}

// the name only labels log lines, so a cut-off one is fine
static void output_name(void *data, struct wl_output *wl, const char *name) {
	(void)wl;
	struct output *output = data;
	snprintf(output->name, sizeof output->name, "%s", name);
	for (char *c = output->name; *c; c++) {
		if ((unsigned char)*c < 0x20 || *c == 0x7f) {
			*c = '?';
		}
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

// the 1x1 buffer is stretched by the viewport, so a new size needs no new
// buffer
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
}

static const struct ext_session_lock_surface_v1_listener lock_surface_listener =
	{
		.configure = lock_surface_configure,
};

bool output_lock(struct output *output) {
	struct display *display = output->display;
	output->surface = wl_compositor_create_surface(display->compositor);
	if (output->surface) {
		output->viewport = wp_viewporter_get_viewport(
			display->viewporter, output->surface);
	}
	if (output->viewport) {
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

// the role object goes before its wl_surface, and both before the output
void output_destroy(struct output *output) {
	if (!output) {
		return;
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
	release(output->wl);
	free(output);
}
