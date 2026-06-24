#include "locky/render.h"
#include "locky/buffer.h"
#include "locky/log.h"
#include <cairo.h>

static void set_source_rgb(cairo_t *cairo, uint32_t color) {
	double r = ((color >> 16) & 0xFF) / 255.0;
	double g = ((color >> 8) & 0xFF) / 255.0;
	double b = (color & 0xFF) / 255.0;
	cairo_set_source_rgb(cairo, r, g, b);
}

static void set_opaque_region(struct locky_output *output) {
	struct wl_region *region =
		wl_compositor_create_region(output->state->compositor);
	wl_region_add(
		region, 0, 0, (int32_t)output->width, (int32_t)output->height);
	wl_surface_set_opaque_region(output->surface, region);
	wl_region_destroy(region);
}

void locky_render_output(struct locky_output *output) {
	struct locky_state *state = output->state;
	if (output->surface == NULL || output->width == 0 ||
		output->height == 0) {
		return;
	}

	int32_t scale = output->scale > 0 ? output->scale : 1;
	int32_t width = (int32_t)output->width * scale;
	int32_t height = (int32_t)output->height * scale;

	struct locky_buffer *buffer = locky_pool_get_buffer(
		output->buffers, state->shm, width, height);
	if (buffer == NULL) {
		locky_log(LOCKY_ERROR, "failed to get buffer for output");
		return;
	}

	cairo_t *cairo = buffer->cairo;
	cairo_identity_matrix(cairo);
	cairo_scale(cairo, scale, scale); // draw in logical coordinates

	set_source_rgb(cairo, state->background);
	cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
	cairo_paint(cairo);
	cairo_surface_flush(buffer->cairo_surface);

	set_opaque_region(output);
	wl_surface_set_buffer_scale(output->surface, scale);
	wl_surface_attach(output->surface, buffer->wl_buffer, 0, 0);
	wl_surface_damage_buffer(output->surface, 0, 0, width, height);
	wl_surface_commit(output->surface);
}
