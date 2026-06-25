#ifndef LOCKY_UI_INPUT_H
#define LOCKY_UI_INPUT_H

#include "locky/locky.h"
#include <cairo.h>

void locky_ui_draw_input(cairo_t *cairo, struct locky_state *state,
	uint32_t width, uint32_t height);

#endif
