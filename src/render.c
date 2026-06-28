#include "locky/render.h"
#include "locky/background.h"
#include "locky/buffer.h"
#include "locky/log.h"
#include "locky/ui/clock.h"
#include "locky/ui/input.h"
#include "locky/ui/user.h"
#include <cairo.h>

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

	locky_background_draw(cairo, state, output->width, output->height);
	locky_ui_draw_clock(cairo, output->width);
	locky_ui_draw_user(cairo, state, output->width, output->height);
	locky_ui_draw_input(cairo, state, output->width, output->height);
	cairo_surface_flush(buffer->cairo_surface);

	set_opaque_region(output);
	wl_surface_set_buffer_scale(output->surface, scale);
	wl_surface_attach(output->surface, buffer->wl_buffer, 0, 0);
	wl_surface_damage_buffer(output->surface, 0, 0, width, height);
	wl_surface_commit(output->surface);
}

void locky_render_all_outputs(struct locky_state *state) {
	struct locky_output *output;
	wl_list_for_each (output, &state->outputs, link) {
		if (output->lock_surface != NULL && output->width != 0) {
			locky_render_output(output);
		}
	}
}
