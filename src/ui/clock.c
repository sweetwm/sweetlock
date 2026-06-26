#include "locky/ui/clock.h"
#include <pango/pangocairo.h>
#include <time.h>

#define CLOCK_Y 42.0
#define DATE_Y 126.0

static void set_source_rgba(cairo_t *cairo, uint32_t color, double alpha) {
	double r = ((color >> 16) & 0xFF) / 255.0;
	double g = ((color >> 8) & 0xFF) / 255.0;
	double b = (color & 0xFF) / 255.0;
	cairo_set_source_rgba(cairo, r, g, b, alpha);
}

static void draw_centered_text(cairo_t *cairo, const char *text,
	const char *font_name, double y, uint32_t width, double alpha) {
	PangoLayout *layout = pango_cairo_create_layout(cairo);
	PangoFontDescription *font =
		pango_font_description_from_string(font_name);
	pango_layout_set_font_description(layout, font);
	pango_layout_set_text(layout, text, -1);
	pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
	pango_layout_set_width(layout, (int)width * PANGO_SCALE);

	int text_width = 0;
	int text_height = 0;
	pango_layout_get_pixel_size(layout, &text_width, &text_height);
	cairo_move_to(cairo, 0.0, y);
	set_source_rgba(cairo, 0xF4F1FF, alpha);
	pango_cairo_show_layout(cairo, layout);

	pango_font_description_free(font);
	g_object_unref(layout);
}

void locky_ui_draw_clock(cairo_t *cairo, uint32_t width) {
	time_t raw = time(NULL);
	struct tm local = {0};
	if (raw == (time_t)-1 || localtime_r(&raw, &local) == NULL) {
		return;
	}

	char clock[16] = {0};
	char date[64] = {0};
	if (strftime(clock, sizeof(clock), "%H:%M", &local) == 0 ||
		strftime(date, sizeof(date), "%A, %B %-d", &local) == 0) {
		return;
	}

	draw_centered_text(cairo, clock, "sans bold 54", CLOCK_Y, width, 0.54);
	draw_centered_text(cairo, date, "sans bold 13", DATE_Y, width, 0.48);
}
