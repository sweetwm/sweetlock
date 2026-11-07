#ifndef SWEETLOCK_BACKGROUND_BLUR_H
#define SWEETLOCK_BACKGROUND_BLUR_H

#include <cairo.h>
#include <stdbool.h>
#include <stdint.h>

enum { BLUR_CHANNELS = 4 };

// how much smaller to blur at, so the cost falls as sigma grows
int32_t blur_factor(double sigma);

bool blur(uint16_t *pixels, int32_t width, int32_t height, double sigma);

bool blur_scale(cairo_surface_t *source, double sigma, int32_t factor,
	cairo_surface_t *target);

#endif
