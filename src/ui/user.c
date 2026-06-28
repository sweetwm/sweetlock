#include "locky/ui/user.h"
#include "locky/ui/layout.h"
#include <pango/pangocairo.h>

static void set_source_rgba(cairo_t *cairo, uint32_t color, double alpha) {
	double r = ((color >> 16) & 0xFF) / 255.0;
	double g = ((color >> 8) & 0xFF) / 255.0;
	double b = (color & 0xFF) / 255.0;
	cairo_set_source_rgba(cairo, r, g, b, alpha);
}

static void draw_avatar_icon(cairo_t *cairo, double cx, double cy) {
	set_source_rgba(cairo, 0xF4F1FF, 0.90);
	cairo_set_line_width(cairo, 6.0);
	cairo_set_line_cap(cairo, CAIRO_LINE_CAP_ROUND);

	cairo_arc(cairo, cx, cy - 16.0, 15.0, 0.0, 6.28318530718);
	cairo_stroke(cairo);

	cairo_arc(cairo, cx, cy + 32.0, 30.0, 3.14159265359, 6.28318530718);
	cairo_stroke(cairo);
}

static void draw_avatar_image(cairo_t *cairo, cairo_surface_t *surface,
	double cx, double cy, double size) {
	double source_width = cairo_image_surface_get_width(surface);
	double source_height = cairo_image_surface_get_height(surface);
	if (source_width <= 0.0 || source_height <= 0.0) {
		return;
	}

	double scale_x = size / source_width;
	double scale_y = size / source_height;
	double scale = scale_x > scale_y ? scale_x : scale_y;
	double draw_width = source_width * scale;
	double draw_height = source_height * scale;

	cairo_save(cairo);
	cairo_arc(cairo, cx, cy, size / 2.0, 0.0, 6.28318530718);
	cairo_clip(cairo);
	cairo_translate(cairo, cx - draw_width / 2.0, cy - draw_height / 2.0);
	cairo_scale(cairo, scale, scale);
	cairo_set_source_surface(cairo, surface, 0.0, 0.0);
	cairo_paint(cairo);
	cairo_restore(cairo);
}

static void draw_name(cairo_t *cairo, const char *name, double y,
	double label_width, uint32_t width) {
	double x = ((double)width - label_width) / 2.0;

	PangoLayout *layout = pango_cairo_create_layout(cairo);
	PangoFontDescription *font =
		pango_font_description_from_string("sans 20");
	pango_layout_set_font_description(layout, font);
	pango_layout_set_text(layout, name, -1);
	pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
	pango_layout_set_width(layout, (int)(label_width * PANGO_SCALE));
	pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);

	int text_width = 0;
	int text_height = 0;
	pango_layout_get_pixel_size(layout, &text_width, &text_height);
	cairo_move_to(cairo, x, y - (double)text_height / 2.0);
	set_source_rgba(cairo, 0xF4F1FF, 0.88);
	pango_cairo_show_layout(cairo, layout);

	pango_font_description_free(font);
	g_object_unref(layout);
}

void locky_ui_draw_user(cairo_t *cairo, struct locky_state *state,
	uint32_t width, uint32_t height) {
	struct locky_ui_layout layout = {0};
	locky_ui_layout_build(width, height, &layout);
	if (!layout.show_user) {
		return;
	}

	set_source_rgba(cairo, 0x4A4564, 0.38);
	cairo_arc(cairo, layout.avatar_cx, layout.avatar_cy,
		layout.avatar_size / 2.0, 0.0, 6.28318530718);
	cairo_fill(cairo);

	if (state->avatar_surface != NULL) {
		draw_avatar_image(cairo, state->avatar_surface,
			layout.avatar_cx, layout.avatar_cy, layout.avatar_size);
	} else {
		draw_avatar_icon(cairo, layout.avatar_cx, layout.avatar_cy);
	}
	draw_name(cairo, state->username, layout.name_y, layout.name_width,
		width);
}
