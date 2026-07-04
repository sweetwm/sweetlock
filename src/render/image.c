#include "render/image.h"
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define IMAGE_MAX_DIMENSION 16384
#define IMAGE_MAX_PIXELS 67108864LL

static bool dimensions_safe(int width, int height) {
	if (width <= 0 || height <= 0) {
		return false;
	}
	if (width > IMAGE_MAX_DIMENSION || height > IMAGE_MAX_DIMENSION) {
		return false;
	}
	return (int64_t)width * (int64_t)height <= IMAGE_MAX_PIXELS;
}

static cairo_surface_t *pixbuf_to_surface(GdkPixbuf *pixbuf) {
	int width = gdk_pixbuf_get_width(pixbuf);
	int height = gdk_pixbuf_get_height(pixbuf);
	int rowstride = gdk_pixbuf_get_rowstride(pixbuf);
	int channels = gdk_pixbuf_get_n_channels(pixbuf);
	bool has_alpha = gdk_pixbuf_get_has_alpha(pixbuf);
	const guchar *pixels = gdk_pixbuf_get_pixels(pixbuf);

	cairo_surface_t *surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(surface);
		return NULL;
	}

	unsigned char *dst = cairo_image_surface_get_data(surface);
	int dst_stride = cairo_image_surface_get_stride(surface);
	for (int y = 0; y < height; y++) {
		const guchar *src_row = pixels + (ptrdiff_t)y * rowstride;
		unsigned char *dst_row = dst + (ptrdiff_t)y * dst_stride;
		for (int x = 0; x < width; x++) {
			const guchar *src = src_row + (ptrdiff_t)x * channels;
			uint32_t alpha = has_alpha ? src[3] : 255;
			uint32_t red = (uint32_t)src[0] * alpha / 255;
			uint32_t green = (uint32_t)src[1] * alpha / 255;
			uint32_t blue = (uint32_t)src[2] * alpha / 255;
			uint32_t pixel = (alpha << 24) | (red << 16) |
					 (green << 8) | blue;
			memcpy(dst_row + (ptrdiff_t)x * 4, &pixel,
				sizeof(pixel));
		}
	}
	cairo_surface_mark_dirty(surface);
	return surface;
}

static cairo_surface_t *load_png_surface(const char *path) {
	cairo_surface_t *surface = cairo_image_surface_create_from_png(path);
	if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(surface);
		return NULL;
	}
	if (!dimensions_safe(cairo_image_surface_get_width(surface),
		    cairo_image_surface_get_height(surface))) {
		cairo_surface_destroy(surface);
		return NULL;
	}
	return surface;
}

cairo_surface_t *locky_image_load(
	const char *path, int width, int height, bool preserve_aspect_ratio) {
	if (path == NULL || path[0] == '\0') {
		return NULL;
	}

	int source_width = 0;
	int source_height = 0;
	if (gdk_pixbuf_get_file_info(path, &source_width, &source_height) !=
			NULL &&
		!dimensions_safe(source_width, source_height)) {
		return NULL;
	}

	GError *error = NULL;
	GdkPixbuf *pixbuf = NULL;
	if (width > 0 || height > 0) {
		pixbuf = gdk_pixbuf_new_from_file_at_scale(
			path, width, height, preserve_aspect_ratio, &error);
	} else {
		pixbuf = gdk_pixbuf_new_from_file(path, &error);
	}
	if (pixbuf == NULL) {
		cairo_surface_t *surface = load_png_surface(path);
		if (surface != NULL) {
			g_clear_error(&error);
			return surface;
		}
		g_clear_error(&error);
		return NULL;
	}
	if (!dimensions_safe(gdk_pixbuf_get_width(pixbuf),
		    gdk_pixbuf_get_height(pixbuf))) {
		g_object_unref(pixbuf);
		return NULL;
	}

	cairo_surface_t *surface = pixbuf_to_surface(pixbuf);
	g_object_unref(pixbuf);
	return surface;
}
