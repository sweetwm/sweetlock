#include "locky/user.h"
#include <cairo.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <pwd.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define AVATAR_LOAD_SIZE 256
#define FACE_PATH_MAX 4096

static void copy_name(char dst[static LOCKY_USERNAME_MAX], const char *src) {
	if (src == NULL || src[0] == '\0') {
		return;
	}
	snprintf(dst, LOCKY_USERNAME_MAX, "%s", src);
}

static void copy_gecos_name(
	char dst[static LOCKY_USERNAME_MAX], const char *gecos) {
	if (gecos == NULL || gecos[0] == '\0') {
		return;
	}

	size_t len = strcspn(gecos, ",");
	if (len == 0) {
		return;
	}
	if (len >= LOCKY_USERNAME_MAX) {
		len = LOCKY_USERNAME_MAX - 1;
	}

	memcpy(dst, gecos, len);
	dst[len] = '\0';
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

static void load_face_avatar(struct locky_state *state, const char *home) {
	if (home == NULL || home[0] == '\0') {
		return;
	}

	char path[FACE_PATH_MAX] = {0};
	int len = snprintf(path, sizeof(path), "%s/.face", home);
	if (len <= 0 || (size_t)len >= sizeof(path)) {
		return;
	}
	if (access(path, R_OK) != 0) {
		return;
	}

	GError *error = NULL;
	GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_scale(
		path, AVATAR_LOAD_SIZE, AVATAR_LOAD_SIZE, true, &error);
	if (pixbuf == NULL) {
		g_clear_error(&error);
		return;
	}

	state->avatar_surface = pixbuf_to_surface(pixbuf);
	g_object_unref(pixbuf);
}

void locky_user_init(struct locky_state *state) {
	copy_name(state->username, "User");
	const char *home = getenv("HOME");

	struct passwd pwd = {0};
	struct passwd *result = NULL;
	long buf_size = sysconf(_SC_GETPW_R_SIZE_MAX);
	if (buf_size < 1024) {
		buf_size = 1024;
	}

	char *buf = malloc((size_t)buf_size);
	if (buf == NULL) {
		copy_name(state->username, getenv("USER"));
		load_face_avatar(state, home);
		return;
	}

	if (getpwuid_r(getuid(), &pwd, buf, (size_t)buf_size, &result) == 0 &&
		result != NULL) {
		copy_name(state->username, pwd.pw_name);
		copy_gecos_name(state->username, pwd.pw_gecos);
		if (home == NULL || home[0] == '\0') {
			home = pwd.pw_dir;
		}
	} else {
		copy_name(state->username, getenv("USER"));
	}

	load_face_avatar(state, home);
	free(buf);
}

void locky_user_finish(struct locky_state *state) {
	if (state->avatar_surface != NULL) {
		cairo_surface_destroy(state->avatar_surface);
		state->avatar_surface = NULL;
	}
}
