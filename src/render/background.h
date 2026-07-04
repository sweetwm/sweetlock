#ifndef LOCKY_BACKGROUND_H
#define LOCKY_BACKGROUND_H

#include "app/state.h"
#include <cairo.h>
#include <stdbool.h>
#include <stdint.h>

struct locky_background_geometry {
	double x;
	double y;
	double width;
	double height;
};

bool locky_background_cover_geometry(int32_t source_width,
	int32_t source_height, uint32_t dest_width, uint32_t dest_height,
	struct locky_background_geometry *geometry);

void locky_background_init(struct locky_state *state);
void locky_background_draw(cairo_t *cairo, struct locky_state *state,
	uint32_t width, uint32_t height);
void locky_background_finish(struct locky_state *state);

#endif
