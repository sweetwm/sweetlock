#ifndef SWEETLOCK_BACKGROUND_DRAW_H
#define SWEETLOCK_BACKGROUND_DRAW_H

#include <cairo.h>
#include <stdbool.h>

#include "config/config.h"

// rgb only; the alpha is ignored
void background_fill(cairo_t *cr, struct color color);

bool background_draw(const struct config *config, cairo_surface_t *target);

#endif
