#include "background/draw.h"

#include <math.h>

enum {
	// stops along smoothstep; linear between them is within a level
	RAMP_STOPS = 16,
};

// 8 bits band on dark gradients; pixman dithers linear and radial patterns
// but not meshes
static const cairo_dither_t dither = CAIRO_DITHER_GOOD;

static double channel(uint8_t value) {
	return value / 255.0;
}

static double mix(uint8_t from, uint8_t to, double t) {
	return channel(from) + (channel(to) - channel(from)) * t;
}

static double smoothstep(double t) {
	return t * t * (3 - 2 * t);
}

static void add_stop(cairo_pattern_t *pattern, double offset, struct color c) {
	cairo_pattern_add_color_stop_rgb(pattern, offset, channel(c.red),
		channel(c.green), channel(c.blue));
}

// patterns work in a unit square stretched over the output, so percent
// positions and sizes follow its shape
static void stretch(cairo_pattern_t *pattern, int width, int height) {
	cairo_matrix_t matrix;
	cairo_matrix_init_scale(&matrix, 1.0 / width, 1.0 / height);
	cairo_pattern_set_matrix(pattern, &matrix);
	cairo_pattern_set_dither(pattern, dither);
}

static void paint(cairo_t *cr, cairo_pattern_t *pattern) {
	cairo_set_source(cr, pattern);
	cairo_paint(cr);
	cairo_pattern_destroy(pattern);
}

static cairo_pattern_t *horizontal(struct color left, struct color right) {
	cairo_pattern_t *pattern = cairo_pattern_create_linear(0, 0, 1, 0);
	add_stop(pattern, 0, left);
	add_stop(pattern, 1, right);
	return pattern;
}

// bilinear: the top edge's gradient, then the bottom edge's through a mask
// that fades in from top to bottom
static void draw_gradient(
	cairo_t *cr, const struct config_gradient *g, int width, int height) {
	cairo_pattern_t *top = horizontal(g->top_left, g->top_right);
	stretch(top, width, height);
	paint(cr, top);
	cairo_pattern_t *bottom = horizontal(g->bottom_left, g->bottom_right);
	cairo_pattern_t *fade = cairo_pattern_create_linear(0, 0, 0, 1);
	cairo_pattern_add_color_stop_rgba(fade, 0, 0, 0, 0, 0);
	cairo_pattern_add_color_stop_rgba(fade, 1, 0, 0, 0, 1);
	stretch(bottom, width, height);
	stretch(fade, width, height);
	cairo_set_source(cr, bottom);
	cairo_mask(cr, fade);
	cairo_pattern_destroy(fade);
	cairo_pattern_destroy(bottom);
}

static double farthest_corner(double x, double y) {
	double dx = fmax(x, 1 - x);
	double dy = fmax(y, 1 - y);
	return sqrt(dx * dx + dy * dy);
}

static void draw_radial(cairo_t *cr, const struct config_radial *radial,
	int width, int height) {
	double x = radial->center_x / 100.0;
	double y = radial->center_y / 100.0;
	double r = farthest_corner(x, y) * radial->radius / 100.0;
	cairo_pattern_t *pattern =
		cairo_pattern_create_radial(x, y, 0, x, y, r);
	for (int i = 0; i <= RAMP_STOPS; i++) {
		double offset = (double)i / (double)RAMP_STOPS;
		double t = smoothstep(offset);
		struct color c = radial->center;
		struct color e = radial->edge;
		cairo_pattern_add_color_stop_rgb(pattern, offset,
			mix(c.red, e.red, t), mix(c.green, e.green, t),
			mix(c.blue, e.blue, t));
	}
	stretch(pattern, width, height);
	paint(cr, pattern);
}

// filled only over its bounding box, since most of the output is outside it
static void draw_blob(
	cairo_t *cr, const struct config_blob *blob, int width, int height) {
	double x = blob->x / 100.0;
	double y = blob->y / 100.0;
	double r = blob->size / 100.0;
	struct color c = blob->color;
	cairo_pattern_t *pattern =
		cairo_pattern_create_radial(x, y, 0, x, y, r);
	for (int i = 0; i <= RAMP_STOPS; i++) {
		double offset = (double)i / (double)RAMP_STOPS;
		cairo_pattern_add_color_stop_rgba(pattern, offset,
			channel(c.red), channel(c.green), channel(c.blue),
			channel(c.alpha) * (1 - smoothstep(offset)));
	}
	cairo_pattern_set_extend(pattern, CAIRO_EXTEND_NONE);
	stretch(pattern, width, height);
	cairo_set_source(cr, pattern);
	cairo_rectangle(cr, (x - r) * width, (y - r) * height, 2 * r * width,
		2 * r * height);
	cairo_fill(cr);
	cairo_pattern_destroy(pattern);
}

static void draw_mode(cairo_t *cr, const struct config *config,
	enum background_mode mode, int width, int height) {
	switch (mode) {
	case BACKGROUND_GRADIENT:
		draw_gradient(cr, &config->background.gradient, width, height);
		return;
	case BACKGROUND_RADIAL:
		draw_radial(cr, &config->background.radial, width, height);
		return;
	case BACKGROUND_SOLID:
	case BACKGROUND_LAYERED:
		break;
	}
	struct color c = config->background.color;
	cairo_set_source_rgb(
		cr, channel(c.red), channel(c.green), channel(c.blue));
	cairo_paint(cr);
}

bool background_draw(const struct config *config, cairo_surface_t *target) {
	int width = cairo_image_surface_get_width(target);
	int height = cairo_image_surface_get_height(target);
	cairo_t *cr = cairo_create(target);
	if (config->background.mode == BACKGROUND_LAYERED) {
		draw_mode(cr, config, config->background.layered.base, width,
			height);
		for (int32_t i = 0; i < config->background.layered.blob_count;
			i++) {
			draw_blob(cr, &config->background.layered.blob[i],
				width, height);
		}
	} else {
		draw_mode(cr, config, config->background.mode, width, height);
	}
	cairo_status_t status = cairo_status(cr);
	cairo_destroy(cr);
	cairo_surface_flush(target);
	return status == CAIRO_STATUS_SUCCESS && width > 0 && height > 0;
}
