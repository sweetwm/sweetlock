#ifndef SWEETLOCK_IMAGE_IMAGE_H
#define SWEETLOCK_IMAGE_IMAGE_H

#include <cairo.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fs/file.h"

enum {
	IMAGE_FILE_MAX = 64 * 1024 * 1024,
	IMAGE_SIDE_MAX = 32767,
	IMAGE_PIXELS_MAX = 1 << 25,
};

// the size from a PNG's signature and IHDR chunk, without checking limits
bool image_png_size(const unsigned char *data, size_t length, uint32_t *width,
	uint32_t *height);

enum image_format {
	IMAGE_UNKNOWN,
	IMAGE_PNG,
	IMAGE_JPEG,
};

// the size from a JPEG's first frame header, without checking limits
bool image_jpeg_size(const unsigned char *data, size_t length, uint32_t *width,
	uint32_t *height);

enum image_format image_probe(const unsigned char *data, size_t length,
	uint32_t *width, uint32_t *height);

cairo_surface_t *image_decode(const unsigned char *data, size_t length,
	double fraction, char *error, size_t error_size);

// image_decode of a file, with its shorter side at least side where the
// format can decode smaller
enum file_result image_load(const char *path, uint32_t side,
	cairo_surface_t **image, char *error, size_t error_size);

#endif
