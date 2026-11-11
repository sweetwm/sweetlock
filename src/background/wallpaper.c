#include "background/wallpaper.h"

#include <math.h>
#include <stdio.h>

#include "background/blur.h"
#include "background/draw.h"
#include "image/image.h"
#include "wayland/scale.h"

// where the image lands on the output, in output pixels
struct placement {
	double scale_x;
	double scale_y;
	double x;
	double y;
};

static struct placement place(enum background_fit fit, double image_width,
	double image_height, double width, double height) {
	double x = width / image_width;
	double y = height / image_height;
	switch (fit) {
	case FIT_FILL:
		x = y = fmax(x, y);
		break;
	case FIT_FIT:
		x = y = fmin(x, y);
		break;
	case FIT_CENTER:
		x = y = 1;
		break;
	case FIT_STRETCH:
		break;
	}
	// whole pixels, so a centred image stays sharp
	return (struct placement){
		.scale_x = x,
		.scale_y = y,
		.x = round((width - image_width * x) / 2),
		.y = round((height - image_height * y) / 2),
	};
}

static void cover(cairo_t *cr, const struct placement *at, double width,
	double height, double output_width, double output_height) {
	const double far = 1 << 20;
	double right = at->x + width * at->scale_x;
	double bottom = at->y + height * at->scale_y;
	right = right < output_width - 0.01 ? right : far;
	bottom = bottom < output_height - 0.01 ? bottom : far;
	cairo_rectangle(cr, at->x, at->y, right - at->x, bottom - at->y);
}

// the base colour shows around a fitted image and through a transparent one
static bool compose(cairo_surface_t *canvas, cairo_surface_t *image,
	const struct config *config, const struct placement *at,
	double image_width, double image_height, int32_t factor,
	cairo_surface_t *target) {
	int width = cairo_image_surface_get_width(image);
	int height = cairo_image_surface_get_height(image);
	cairo_t *cr = cairo_create(canvas);
	background_fill(cr, config->background.color);
	cairo_scale(cr, 1.0 / factor, 1.0 / factor);
	cairo_translate(cr, at->x, at->y);
	// a JPEG may have been decoded smaller than its size
	cairo_scale(cr, at->scale_x * image_width / width,
		at->scale_y * image_height / height);
	// the pattern keeps this transform; the cover is in output pixels
	cairo_set_source_surface(cr, image, 0, 0);
	cairo_identity_matrix(cr);
	cairo_scale(cr, 1.0 / factor, 1.0 / factor);
	cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_GOOD);
	// edge pixels repeat outward instead of fading to the base colour
	cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_PAD);
	cover(cr, at, image_width, image_height,
		cairo_image_surface_get_width(target),
		cairo_image_surface_get_height(target));
	cairo_fill(cr);
	cairo_set_source_rgba(
		cr, 0, 0, 0, config->background.image.dim / 100.0);
	cairo_paint(cr);
	cairo_status_t status = cairo_status(cr);
	cairo_destroy(cr);
	cairo_surface_flush(canvas);
	return status == CAIRO_STATUS_SUCCESS;
}

// a blurred image is composed, dimmed and blurred at a reduced size, then
// scaled up, so the cost falls as the blur grows
static bool draw(cairo_surface_t *image, const struct config *config,
	const struct placement *at, double image_width, double image_height,
	double sigma, cairo_surface_t *target) {
	int32_t factor = blur_factor(sigma);
	cairo_surface_t *canvas = target;
	if (factor > 1) {
		int width = cairo_image_surface_get_width(target);
		int height = cairo_image_surface_get_height(target);
		canvas = cairo_image_surface_create(CAIRO_FORMAT_RGB24,
			(width + factor - 1) / factor,
			(height + factor - 1) / factor);
	}
	bool ok = cairo_surface_status(canvas) == CAIRO_STATUS_SUCCESS &&
		  compose(canvas, image, config, at, image_width, image_height,
			  factor, target) &&
		  (sigma <= 0 ||
			  blur_scale(canvas, sigma / factor, factor, target));
	if (canvas != target) {
		cairo_surface_destroy(canvas);
	}
	return ok;
}

bool wallpaper_draw(const unsigned char *data, size_t length,
	const struct config *config, uint32_t scale, cairo_surface_t *target,
	char *error, size_t error_size) {
	uint32_t image_width = 0;
	uint32_t image_height = 0;
	image_probe(data, length, &image_width, &image_height);
	double sigma = config->background.image.blur * scale_factor(scale);
	struct placement at = {0};
	double fraction = 1;
	if (image_width > 0 && image_height > 0) {
		at = place(config->background.image.fit, image_width,
			image_height, cairo_image_surface_get_width(target),
			cairo_image_surface_get_height(target));
		fraction = fmax(at.scale_x, at.scale_y) / blur_factor(sigma);
	}
	cairo_surface_t *image =
		image_decode(data, length, fraction, error, error_size);
	if (!image) {
		return false;
	}
	bool ok = draw(
		image, config, &at, image_width, image_height, sigma, target);
	cairo_surface_destroy(image);
	if (!ok) {
		snprintf(error, error_size, "failed to draw it");
	}
	return ok;
}
