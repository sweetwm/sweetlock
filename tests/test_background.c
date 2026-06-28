#include "locky/background.h"
#include <assert.h>
#include <math.h>

static bool near(double actual, double expected) {
	return fabs(actual - expected) < 0.001;
}

static void test_wide_image_crops_sides(void) {
	struct locky_background_geometry geometry = {0};

	assert(locky_background_cover_geometry(
		1920, 1080, 1000, 1000, &geometry));
	assert(near(geometry.width, 1777.777));
	assert(near(geometry.height, 1000.0));
	assert(near(geometry.x, -388.888));
	assert(near(geometry.y, 0.0));
}

static void test_tall_image_crops_top_and_bottom(void) {
	struct locky_background_geometry geometry = {0};

	assert(locky_background_cover_geometry(
		1080, 1920, 1000, 1000, &geometry));
	assert(near(geometry.width, 1000.0));
	assert(near(geometry.height, 1777.777));
	assert(near(geometry.x, 0.0));
	assert(near(geometry.y, -388.888));
}

static void test_invalid_dimensions_fail(void) {
	struct locky_background_geometry geometry = {0};

	assert(!locky_background_cover_geometry(0, 100, 100, 100, &geometry));
	assert(!locky_background_cover_geometry(100, 0, 100, 100, &geometry));
	assert(!locky_background_cover_geometry(100, 100, 0, 100, &geometry));
	assert(!locky_background_cover_geometry(100, 100, 100, 0, &geometry));
	assert(!locky_background_cover_geometry(100, 100, 100, 100, NULL));
}

int main(void) {
	test_wide_image_crops_sides();
	test_tall_image_crops_top_and_bottom();
	test_invalid_dimensions_fail();
	return 0;
}
