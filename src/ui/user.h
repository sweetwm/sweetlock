#ifndef LOCKY_UI_USER_H
#define LOCKY_UI_USER_H

#include "app/state.h"
#include <cairo.h>

void locky_ui_draw_user(cairo_t *cairo, struct locky_state *state,
	uint32_t width, uint32_t height);

#endif
