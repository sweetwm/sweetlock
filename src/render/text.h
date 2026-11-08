#ifndef SWEETLOCK_RENDER_TEXT_H
#define SWEETLOCK_RENDER_TEXT_H

#include <cairo.h>
#include <pango/pango.h>

// fontconfig loads in Pango's own thread from here; this returns at once
void text_prepare(void);

// one line, ellipsized at its width, with tabular figures
PangoLayout *text_layout_create(void);

// the layout reshapes on its next use
void text_set_font(PangoLayout *layout, const char *family, PangoWeight weight,
	double pixels, double width);

// sets the text only when it differs, so an unchanged layout stays shaped
void text_set(PangoLayout *layout, const char *text);

// x is the left edge of the text, snapped to the pixel grid like the baseline
void text_draw(cairo_t *cr, PangoLayout *layout, double x, double centre_y);

void text_draw_centred(
	cairo_t *cr, PangoLayout *layout, double width, double centre_y);

#endif
