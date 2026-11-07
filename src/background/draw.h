#ifndef SWEETLOCK_BACKGROUND_DRAW_H
#define SWEETLOCK_BACKGROUND_DRAW_H

#include <cairo.h>
#include <stdbool.h>

#include "config.h"

bool background_draw(const struct config *config, cairo_surface_t *target);

#endif
