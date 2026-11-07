#ifndef SWEETLOCK_IMAGE_H
#define SWEETLOCK_IMAGE_H

#include <cairo.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "file.h"

enum {
	IMAGE_FILE_MAX = 64 * 1024 * 1024,
	IMAGE_SIDE_MAX = 32767,
	IMAGE_PIXELS_MAX = 1 << 25,
};

// the size from a PNG's signature and IHDR chunk, without checking limits
bool image_png_size(const unsigned char *data, size_t length, uint32_t *width,
	uint32_t *height);

// decodes a PNG within the limits above; safe on any thread
enum file_result image_load(const char *path, cairo_surface_t **image,
	char *error, size_t error_size);

#endif
