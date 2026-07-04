#include "render/image.h"
#include <assert.h>
#include <cairo.h>
#include <stdlib.h>
#include <unistd.h>

static void write_test_png(const char *path) {
	cairo_surface_t *surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 2, 3);
	assert(cairo_surface_status(surface) == CAIRO_STATUS_SUCCESS);

	cairo_t *cairo = cairo_create(surface);
	cairo_set_source_rgb(cairo, 1.0, 0.0, 0.0);
	cairo_paint(cairo);
	cairo_destroy(cairo);

	assert(cairo_surface_write_to_png(surface, path) ==
		CAIRO_STATUS_SUCCESS);
	cairo_surface_destroy(surface);
}

static void test_load_png(void) {
	char path[] = "/tmp/locky-image-test-XXXXXX.png";
	int fd = mkstemps(path, 4);
	assert(fd >= 0);
	close(fd);

	write_test_png(path);

	cairo_surface_t *surface = locky_image_load(path, 0, 0, true);
	assert(surface != NULL);
	assert(cairo_image_surface_get_width(surface) == 2);
	assert(cairo_image_surface_get_height(surface) == 3);

	cairo_surface_destroy(surface);
	unlink(path);
}

static void test_missing_image_fails(void) {
	assert(locky_image_load("/tmp/locky-missing-image", 0, 0, true) ==
		NULL);
}

int main(void) {
	test_load_png();
	test_missing_image_fails();
	return 0;
}
