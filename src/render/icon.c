#include "render/icon.h"

#include <math.h>

// proportions of the outline in default.png, as fractions of the radius
void icon_person(cairo_t *cr, double x, double y, double radius) {
	cairo_save(cr);
	cairo_set_line_width(cr, radius * 0.08);
	cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
	cairo_new_path(cr);
	cairo_arc(cr, x, y - radius * 0.25, radius * 0.215, 0, 2 * M_PI);
	cairo_stroke(cr);
	// the shoulders are half an ellipse, stroked after the scale is undone
	cairo_save(cr);
	cairo_translate(cr, x, y + radius * 0.49);
	cairo_scale(cr, radius * 0.4, radius * 0.37);
	cairo_arc(cr, 0, 0, 1, M_PI, 2 * M_PI);
	cairo_restore(cr);
	cairo_stroke(cr);
	cairo_restore(cr);
}

static void eye_path(cairo_t *cr) {
	cairo_move_to(cr, -9.5, 0);
	cairo_curve_to(cr, -4.75, -9.5, 4.75, -9.5, 9.5, 0);
	cairo_curve_to(cr, 4.75, 9.5, -4.75, 9.5, -9.5, 0);
	cairo_close_path(cr);
	// even-odd: a ring cut out around a solid pupil
	cairo_new_sub_path(cr);
	cairo_arc(cr, 0, 0, 4, 0, 2 * M_PI);
	cairo_new_sub_path(cr);
	cairo_arc(cr, 0, 0, 2.5, 0, 2 * M_PI);
}

// everything but a band along the slash, so the slash stands apart
static void clip_band(cairo_t *cr) {
	cairo_rectangle(cr, -12, -12, 24, 24);
	cairo_move_to(cr, -10.5, -6.5);
	cairo_line_to(cr, -6.5, -10.5);
	cairo_line_to(cr, 10.5, 6.5);
	cairo_line_to(cr, 6.5, 10.5);
	cairo_close_path(cr);
	cairo_clip(cr);
}

void icon_eye(cairo_t *cr, double x, double y, double unit, bool slashed) {
	cairo_save(cr);
	cairo_translate(cr, x, y);
	cairo_scale(cr, unit, unit);
	cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
	cairo_new_path(cr);
	cairo_save(cr);
	if (slashed) {
		clip_band(cr);
	}
	eye_path(cr);
	cairo_fill(cr);
	cairo_restore(cr);
	if (slashed) {
		cairo_set_line_width(cr, 1.8);
		cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
		cairo_move_to(cr, -7.5, -7.5);
		cairo_line_to(cr, 7.5, 7.5);
		cairo_stroke(cr);
	}
	cairo_restore(cr);
}
