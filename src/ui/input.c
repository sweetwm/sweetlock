#include "locky/ui/input.h"
#include "locky/password.h"
#include "locky/ui/layout.h"
#include <pango/pangocairo.h>

#define INPUT_RADIUS 8.0
#define INPUT_PAD_X 18.0
#define DOT_RADIUS 4.0
#define DOT_GAP 14.0

static void set_source_rgba(cairo_t *cairo, uint32_t color, double alpha) {
	double r = ((color >> 16) & 0xFF) / 255.0;
	double g = ((color >> 8) & 0xFF) / 255.0;
	double b = (color & 0xFF) / 255.0;
	cairo_set_source_rgba(cairo, r, g, b, alpha);
}

static void rounded_rect(
	cairo_t *cairo, double x, double y, double width, double height) {
	double r = INPUT_RADIUS;
	cairo_new_sub_path(cairo);
	cairo_arc(cairo, x + width - r, y + r, r, -1.57079632679, 0.0);
	cairo_arc(cairo, x + width - r, y + height - r, r, 0.0, 1.57079632679);
	cairo_arc(
		cairo, x + r, y + height - r, r, 1.57079632679, 3.14159265359);
	cairo_arc(cairo, x + r, y + r, r, 3.14159265359, 4.71238898038);
	cairo_close_path(cairo);
}

static void draw_label(cairo_t *cairo, const char *text, double x, double y,
	double width, uint32_t color, double alpha) {
	PangoLayout *layout = pango_cairo_create_layout(cairo);
	PangoFontDescription *font =
		pango_font_description_from_string("sans 12");
	pango_layout_set_font_description(layout, font);
	pango_layout_set_text(layout, text, -1);
	pango_layout_set_width(layout, (int)(width * PANGO_SCALE));
	pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);

	int text_width = 0;
	int text_height = 0;
	pango_layout_get_pixel_size(layout, &text_width, &text_height);
	cairo_move_to(cairo, x, y - (double)text_height / 2.0);
	set_source_rgba(cairo, color, alpha);
	pango_cairo_show_layout(cairo, layout);

	pango_font_description_free(font);
	g_object_unref(layout);
}

static void draw_dots(
	cairo_t *cairo, double x, double y, double width, size_t count) {
	size_t max_dots = (size_t)(width / DOT_GAP);
	size_t dots = count > max_dots ? max_dots : count;

	set_source_rgba(cairo, 0xF4F1FF, 0.86);
	for (size_t i = 0; i < dots; i++) {
		double cx = x + (double)i * DOT_GAP + DOT_RADIUS;
		cairo_arc(cairo, cx, y, DOT_RADIUS, 0.0, 6.28318530718);
		cairo_fill(cairo);
	}
}

void locky_ui_draw_input(cairo_t *cairo, struct locky_state *state,
	uint32_t width, uint32_t height) {
	struct locky_ui_layout layout = {0};
	locky_ui_layout_build(width, height, &layout);
	double x = layout.input_x;
	double y = layout.input_y;

	rounded_rect(cairo, x, y, layout.input_width, layout.input_height);
	set_source_rgba(cairo, 0x4A4564, 0.66);
	cairo_fill_preserve(cairo);

	uint32_t border = 0xFFFFFF;
	double border_alpha = 0.08;
	if (state->auth_state == LOCKY_AUTH_VALIDATING) {
		border = 0x89B4FA;
		border_alpha = 0.55;
	} else if (state->auth_state == LOCKY_AUTH_INVALID) {
		border = 0xF38BA8;
		border_alpha = 0.70;
	}
	cairo_set_line_width(cairo, 1.0);
	set_source_rgba(cairo, border, border_alpha);
	cairo_stroke(cairo);

	double text_x = x + INPUT_PAD_X;
	double text_y = y + layout.input_height / 2.0;
	double text_width = layout.input_width - INPUT_PAD_X * 2.0;
	size_t password_len = locky_password_length(&state->password);

	if (state->auth_state == LOCKY_AUTH_VALIDATING) {
		draw_label(cairo, "Checking...", text_x, text_y, text_width,
			0xDCE6FF, 0.86);
	} else if (state->auth_state == LOCKY_AUTH_INVALID &&
		   password_len == 0) {
		draw_label(cairo, "Wrong password", text_x, text_y, text_width,
			0xFFD6DF, 0.88);
	} else if (password_len == 0) {
		draw_label(cairo, "Password", text_x, text_y, text_width,
			0xF4F1FF, 0.72);
	} else {
		draw_dots(cairo, text_x, text_y, text_width, password_len);
	}
}
