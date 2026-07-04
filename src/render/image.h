#ifndef LOCKY_IMAGE_H
#define LOCKY_IMAGE_H

#include <cairo.h>
#include <stdbool.h>

cairo_surface_t *locky_image_load(
	const char *path, int width, int height, bool preserve_aspect_ratio);

#endif
