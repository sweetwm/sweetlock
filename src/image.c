#include "image.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
	PNG_SIGNATURE_SIZE = 8,
	IHDR_LENGTH = 13,
	// signature, chunk length and type, width, height
	PNG_HEADER_SIZE = PNG_SIGNATURE_SIZE + 8 + 8,
};

static uint32_t big_endian(const unsigned char *bytes) {
	return (uint32_t)bytes[0] << 24 | (uint32_t)bytes[1] << 16 |
	       (uint32_t)bytes[2] << 8 | (uint32_t)bytes[3];
}

bool image_png_size(const unsigned char *data, size_t length, uint32_t *width,
	uint32_t *height) {
	static const unsigned char signature[PNG_SIGNATURE_SIZE] = {
		0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
	// IHDR must be the first chunk, so libpng reads these same bytes
	if (length < PNG_HEADER_SIZE ||
		memcmp(data, signature, sizeof signature) != 0 ||
		big_endian(data + 8) != IHDR_LENGTH ||
		memcmp(data + 12, "IHDR", 4) != 0) {
		return false;
	}
	*width = big_endian(data + 16);
	*height = big_endian(data + 20);
	return true;
}

struct reader {
	const unsigned char *data;
	size_t length;
	size_t offset;
};

static cairo_status_t read_png(
	void *closure, unsigned char *out, unsigned int length) {
	struct reader *reader = closure;
	if (length > reader->length - reader->offset) {
		return CAIRO_STATUS_READ_ERROR;
	}
	memcpy(out, reader->data + reader->offset, length);
	reader->offset += length;
	return CAIRO_STATUS_SUCCESS;
}

static bool fits(uint32_t width, uint32_t height) {
	return width > 0 && height > 0 && width <= IMAGE_SIDE_MAX &&
	       height <= IMAGE_SIDE_MAX &&
	       (uint64_t)width * height <= IMAGE_PIXELS_MAX;
}

static cairo_surface_t *decode(const unsigned char *data, size_t length,
	char *error, size_t error_size) {
	uint32_t width = 0;
	uint32_t height = 0;
	if (!image_png_size(data, length, &width, &height)) {
		snprintf(error, error_size, "not a PNG image");
		return NULL;
	}
	if (!fits(width, height)) {
		snprintf(error, error_size,
			"%" PRIu32 "x%" PRIu32
			" is past the limit of %d pixels",
			width, height, IMAGE_PIXELS_MAX);
		return NULL;
	}
	struct reader reader = {.data = data, .length = length};
	cairo_surface_t *image =
		cairo_image_surface_create_from_png_stream(read_png, &reader);
	cairo_status_t status = cairo_surface_status(image);
	if (status == CAIRO_STATUS_SUCCESS &&
		((uint32_t)cairo_image_surface_get_width(image) != width ||
			(uint32_t)cairo_image_surface_get_height(image) !=
				height)) {
		status = CAIRO_STATUS_INVALID_SIZE;
	}
	if (status != CAIRO_STATUS_SUCCESS) {
		snprintf(error, error_size, "failed to decode: %s",
			cairo_status_to_string(status));
		cairo_surface_destroy(image);
		return NULL;
	}
	return image;
}

enum file_result image_load(const char *path, cairo_surface_t **image,
	char *error, size_t error_size) {
	*image = NULL;
	char *data = NULL;
	size_t length = 0;
	enum file_result result = file_read(
		path, IMAGE_FILE_MAX, &data, &length, error, error_size);
	if (result == FILE_OK) {
		*image = decode(
			(const unsigned char *)data, length, error, error_size);
		result = *image ? FILE_OK : FILE_FAILED;
	}
	free(data);
	return result;
}
