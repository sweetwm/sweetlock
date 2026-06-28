#include "locky/ui/layout.h"

#define INPUT_WIDTH 310.0
#define INPUT_HEIGHT 54.0
#define INPUT_MARGIN 32.0
#define INPUT_CENTER_Y 0.575
#define AVATAR_SIZE 112.0
#define NAME_WIDTH 360.0
#define NAME_TO_INPUT 54.0
#define AVATAR_TO_NAME 64.0
#define USER_TOP_MIN 150.0

static double clamp(double value, double min, double max) {
	if (value < min) {
		return min;
	}
	if (value > max) {
		return max;
	}
	return value;
}

void locky_ui_layout_build(
	uint32_t width, uint32_t height, struct locky_ui_layout *layout) {
	double screen_width = (double)width;
	double screen_height = (double)height;
	double input_width =
		clamp(screen_width - INPUT_MARGIN * 2.0, 180.0, INPUT_WIDTH);
	double input_y = screen_height * INPUT_CENTER_Y - INPUT_HEIGHT / 2.0;
	double input_min_y = 174.0;
	double input_max_y = screen_height - INPUT_HEIGHT - 72.0;
	if (input_max_y < input_min_y) {
		input_min_y = input_max_y > 0.0 ? input_max_y : 0.0;
	}
	input_y = clamp(input_y, input_min_y, input_max_y);

	layout->input_x = (screen_width - input_width) / 2.0;
	layout->input_y = input_y;
	layout->input_width = input_width;
	layout->input_height = INPUT_HEIGHT;
	layout->avatar_cx = screen_width / 2.0;
	layout->avatar_size = AVATAR_SIZE;
	layout->name_y = input_y - NAME_TO_INPUT;
	layout->name_width =
		screen_width < NAME_WIDTH ? screen_width : NAME_WIDTH;
	layout->avatar_cy = layout->name_y - AVATAR_TO_NAME - AVATAR_SIZE / 2.0;
	layout->show_user =
		layout->avatar_cy - AVATAR_SIZE / 2.0 >= USER_TOP_MIN;
}
