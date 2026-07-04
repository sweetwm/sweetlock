#include "render/background.h"
#include "app/log.h"
#include "render/image.h"
#include <stdlib.h>
#include <unistd.h>

static void set_source_rgb(cairo_t *cairo, uint32_t color) {
	double r = ((color >> 16) & 0xFF) / 255.0;
	double g = ((color >> 8) & 0xFF) / 255.0;
	double b = (color & 0xFF) / 255.0;
	cairo_set_source_rgb(cairo, r, g, b);
}

bool locky_background_cover_geometry(int32_t source_width,
	int32_t source_height, uint32_t dest_width, uint32_t dest_height,
	struct locky_background_geometry *geometry) {
	if (source_width <= 0 || source_height <= 0 || dest_width == 0 ||
		dest_height == 0 || geometry == NULL) {
		return false;
	}

	double scale_x = (double)dest_width / (double)source_width;
	double scale_y = (double)dest_height / (double)source_height;
	double scale = scale_x > scale_y ? scale_x : scale_y;
	geometry->width = (double)source_width * scale;
	geometry->height = (double)source_height * scale;
	geometry->x = ((double)dest_width - geometry->width) / 2.0;
	geometry->y = ((double)dest_height - geometry->height) / 2.0;
	return true;
}

void locky_background_init(struct locky_state *state) {
	const char *path = getenv("LOCKY_BACKGROUND_IMAGE");
	if (path == NULL || path[0] == '\0') {
		return;
	}
	if (access(path, R_OK) != 0) {
		locky_log(LOCKY_ERROR, "background image is not readable");
		return;
	}

	state->background_surface = locky_image_load(path, 0, 0, true);
	if (state->background_surface != NULL) {
		locky_log(LOCKY_INFO, "loaded background image");
	} else {
		locky_log(LOCKY_ERROR, "failed to load background image");
	}
}

void locky_background_draw(cairo_t *cairo, struct locky_state *state,
	uint32_t width, uint32_t height) {
	set_source_rgb(cairo, state->background);
	cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
	cairo_paint(cairo);
	cairo_set_operator(cairo, CAIRO_OPERATOR_OVER);

	if (state->background_surface == NULL) {
		return;
	}

	int32_t source_width =
		cairo_image_surface_get_width(state->background_surface);
	int32_t source_height =
		cairo_image_surface_get_height(state->background_surface);
	struct locky_background_geometry geometry = {0};
	if (!locky_background_cover_geometry(
		    source_width, source_height, width, height, &geometry)) {
		return;
	}

	cairo_save(cairo);
	cairo_translate(cairo, geometry.x, geometry.y);
	cairo_scale(cairo, geometry.width / (double)source_width,
		geometry.height / (double)source_height);
	cairo_set_source_surface(cairo, state->background_surface, 0.0, 0.0);
	cairo_pattern_set_filter(cairo_get_source(cairo), CAIRO_FILTER_GOOD);
	cairo_paint(cairo);
	cairo_restore(cairo);
}

void locky_background_finish(struct locky_state *state) {
	if (state->background_surface != NULL) {
		cairo_surface_destroy(state->background_surface);
		state->background_surface = NULL;
	}
}
