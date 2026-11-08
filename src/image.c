#include "image.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jpeg.h"

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

static uint32_t big_endian16(const unsigned char *bytes) {
	return (uint32_t)bytes[0] << 8 | bytes[1];
}

// walks the marker segments to the first frame header, as libjpeg does
bool image_jpeg_size(const unsigned char *data, size_t length, uint32_t *width,
	uint32_t *height) {
	if (length < 2 || data[0] != 0xff || data[1] != 0xd8) {
		return false;
	}
	size_t at = 2;
	while (length - at >= 4 && data[at] == 0xff) {
		unsigned char marker = data[at + 1];
		// fill bytes, and markers with no segment
		if (marker == 0xff || marker == 0x01 ||
			(marker >= 0xd0 && marker <= 0xd7)) {
			at += marker == 0xff ? 1 : 2;
			continue;
		}
		// SOF0 to SOF15, which leave out DHT, JPG and DAC
		bool frame = marker >= 0xc0 && marker <= 0xcf &&
			     marker != 0xc4 && marker != 0xc8 && marker != 0xcc;
		size_t segment = big_endian16(data + at + 2);
		if (frame && segment >= 8 && length - at >= 9) {
			*height = big_endian16(data + at + 5);
			*width = big_endian16(data + at + 7);
			return true;
		}
		// a scan or the end before any frame, or a truncated segment
		if (frame || marker == 0xda || marker == 0xd9 || segment < 2 ||
			segment > length - at - 2) {
			return false;
		}
		at += 2 + segment;
	}
	return false;
}

enum image_format image_probe(const unsigned char *data, size_t length,
	uint32_t *width, uint32_t *height) {
	if (image_png_size(data, length, width, height)) {
		return IMAGE_PNG;
	}
	if (image_jpeg_size(data, length, width, height)) {
		return IMAGE_JPEG;
	}
	return IMAGE_UNKNOWN;
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

static cairo_surface_t *decode_png(const unsigned char *data, size_t length,
	uint32_t width, uint32_t height, char *error, size_t error_size) {
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

cairo_surface_t *image_decode(const unsigned char *data, size_t length,
	double fraction, char *error, size_t error_size) {
	uint32_t width = 0;
	uint32_t height = 0;
	enum image_format format = image_probe(data, length, &width, &height);
	if (format == IMAGE_UNKNOWN) {
		snprintf(error, error_size, "not a PNG or JPEG image");
		return NULL;
	}
	if (!fits(width, height)) {
		snprintf(error, error_size,
			"%" PRIu32 "x%" PRIu32
			" is past the limit of %d pixels",
			width, height, IMAGE_PIXELS_MAX);
		return NULL;
	}
	if (format == IMAGE_PNG) {
		return decode_png(
			data, length, width, height, error, error_size);
	}
	return image_jpeg_decode(
		data, length, width, height, fraction, error, error_size);
}

enum file_result image_load(const char *path, uint32_t side,
	cairo_surface_t **image, char *error, size_t error_size) {
	*image = NULL;
	char *data = NULL;
	size_t length = 0;
	enum file_result result = file_read(
		path, IMAGE_FILE_MAX, &data, &length, error, error_size);
	if (result == FILE_OK) {
		uint32_t width = 0;
		uint32_t height = 0;
		image_probe(
			(const unsigned char *)data, length, &width, &height);
		uint32_t shorter = width < height ? width : height;
		double fraction = shorter > side ? (double)side / shorter : 1;
		*image = image_decode((const unsigned char *)data, length,
			fraction, error, error_size);
		result = *image ? FILE_OK : FILE_FAILED;
	}
	free(data);
	return result;
}
