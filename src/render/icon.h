#ifndef SWEETLOCK_RENDER_ICON_H
#define SWEETLOCK_RENDER_ICON_H

#include <cairo.h>
#include <stdbool.h>

// both draw with the current source

// a head and shoulders outline sized for a circle of the given radius
void icon_person(cairo_t *cr, double x, double y, double radius);

// an eye about 19 units wide around (x, y), struck through when slashed
void icon_eye(cairo_t *cr, double x, double y, double unit, bool slashed);

#endif
