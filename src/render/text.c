#include "render/text.h"

#include <math.h>
#include <pango/pangocairo.h>
#include <string.h>

void text_prepare(void) {
	pango_cairo_font_map_get_default();
}

PangoLayout *text_layout_create(void) {
	PangoContext *context = pango_font_map_create_context(
		pango_cairo_font_map_get_default());
	if (!context) {
		return NULL;
	}
	PangoLayout *layout = pango_layout_new(context);
	g_object_unref(context);
	if (!layout) {
		return NULL;
	}
	pango_layout_set_single_paragraph_mode(layout, TRUE);
	pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
	// digits that change in place keep their width
	PangoAttrList *attributes = pango_attr_list_new();
	pango_attr_list_insert(
		attributes, pango_attr_font_features_new("tnum"));
	pango_layout_set_attributes(layout, attributes);
	pango_attr_list_unref(attributes);
	return layout;
}

void text_set_font(PangoLayout *layout, const char *family, PangoWeight weight,
	double pixels, double width) {
	PangoFontDescription *font = pango_font_description_new();
	pango_font_description_set_family(font, family);
	pango_font_description_set_weight(font, weight);
	pango_font_description_set_absolute_size(font, pixels * PANGO_SCALE);
	pango_layout_set_font_description(layout, font);
	pango_font_description_free(font);
	pango_layout_set_width(layout, (int)(width * PANGO_SCALE));
}

void text_set(PangoLayout *layout, const char *text) {
	if (strcmp(pango_layout_get_text(layout), text) != 0) {
		pango_layout_set_text(layout, text, -1);
	}
}

void text_draw(cairo_t *cr, PangoLayout *layout, double x, double centre_y) {
	PangoRectangle logical;
	pango_layout_get_extents(layout, NULL, &logical);
	double scale = PANGO_SCALE;
	double baseline = pango_layout_get_baseline(layout) / scale;
	double top = centre_y - logical.height / scale / 2 - logical.y / scale;
	// the origin may stay fractional so the baseline lands exactly
	cairo_move_to(cr, round(x - logical.x / scale),
		round(top + baseline) - baseline);
	pango_cairo_show_layout(cr, layout);
}

void text_draw_centred(
	cairo_t *cr, PangoLayout *layout, double width, double centre_y) {
	PangoRectangle logical;
	pango_layout_get_extents(layout, NULL, &logical);
	text_draw(cr, layout, (width - logical.width / (double)PANGO_SCALE) / 2,
		centre_y);
}
