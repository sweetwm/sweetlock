#ifndef SWEETLOCK_IMAGE_IMAGE_H
#define SWEETLOCK_IMAGE_IMAGE_H

#include <cairo.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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

// within the side and pixel limits every decode checks
bool image_fits(uint32_t width, uint32_t height);

// which limit a size image_fits refused is past
void image_limit_error(
	uint32_t width, uint32_t height, char *error, size_t error_size);

enum image_format image_probe(const unsigned char *data, size_t length,
	uint32_t *width, uint32_t *height);

cairo_surface_t *image_decode(const unsigned char *data, size_t length,
	double fraction, char *error, size_t error_size);

// image_decode with the shorter side at least side where the format can
// decode smaller
cairo_surface_t *image_decode_side(const unsigned char *data, size_t length,
	uint32_t side, char *error, size_t error_size);

#endif
