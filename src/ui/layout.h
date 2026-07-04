#ifndef LOCKY_UI_LAYOUT_H
#define LOCKY_UI_LAYOUT_H

#include <stdbool.h>
#include <stdint.h>

struct locky_ui_layout {
	double input_x;
	double input_y;
	double input_width;
	double input_height;
	double avatar_cx;
	double avatar_cy;
	double avatar_size;
	double name_y;
	double name_width;
	bool show_user;
};

void locky_ui_layout_build(
	uint32_t width, uint32_t height, struct locky_ui_layout *layout);

#endif
