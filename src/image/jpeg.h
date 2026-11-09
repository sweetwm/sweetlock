#ifndef SWEETLOCK_IMAGE_JPEG_H
#define SWEETLOCK_IMAGE_JPEG_H

#include <cairo.h>
#include <stddef.h>
#include <stdint.h>

cairo_surface_t *image_jpeg_decode(const unsigned char *data, size_t length,
	uint32_t width, uint32_t height, double fraction, char *error,
	size_t error_size);

#endif
