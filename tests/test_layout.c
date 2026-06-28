#include "locky/ui/layout.h"
#include <assert.h>
#include <math.h>

static bool near(double actual, double expected) {
	return fabs(actual - expected) < 0.001;
}

static void test_desktop_layout(void) {
	struct locky_ui_layout layout = {0};
	locky_ui_layout_build(1920, 1080, &layout);

	assert(near(layout.input_width, 310.0));
	assert(near(layout.input_height, 54.0));
	assert(near(layout.input_x, 805.0));
	assert(near(layout.input_y, 594.0));
	assert(near(layout.avatar_cx, 960.0));
	assert(layout.show_user);
}

static void test_narrow_layout_keeps_min_input_width(void) {
	struct locky_ui_layout layout = {0};
	locky_ui_layout_build(200, 800, &layout);

	assert(near(layout.input_width, 180.0));
	assert(near(layout.input_x, 10.0));
	assert(layout.show_user);
}

static void test_small_layout_hides_user_block(void) {
	struct locky_ui_layout layout = {0};
	locky_ui_layout_build(320, 240, &layout);

	assert(near(layout.input_width, 256.0));
	assert(near(layout.input_y, 114.0));
	assert(!layout.show_user);
}

int main(void) {
	test_desktop_layout();
	test_narrow_layout_keeps_min_input_width();
	test_small_layout_hides_user_block();
	return 0;
}
