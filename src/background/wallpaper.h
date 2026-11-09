#ifndef SWEETLOCK_BACKGROUND_WALLPAPER_H
#define SWEETLOCK_BACKGROUND_WALLPAPER_H

#include <cairo.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "config/config.h"

bool wallpaper_draw(const unsigned char *data, size_t length,
	const struct config *config, uint32_t scale, cairo_surface_t *target,
	char *error, size_t error_size);

#endif
