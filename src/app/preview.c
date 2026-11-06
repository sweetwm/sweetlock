#include <stdlib.h>

#include "app.h"
#include "log.h"
#include "scale.h"
#include "ui/clock.h"
#include "ui/ui.h"

static void draw_part(const struct ui *ui, enum ui_part part, cairo_t *cr,
	int32_t width, int32_t height) {
	struct ui_rect rect;
	if (!ui_shows(ui->content, part) ||
		!ui_place(part, width, height, ui->scale, &rect)) {
		return;
	}
	int32_t physical_width = scale_physical(ui->scale, rect.width);
	cairo_save(cr);
	cairo_translate(cr, scale_physical(ui->scale, rect.x),
		scale_physical(ui->scale, rect.y));
	// the same bounds as the part's own buffer
	cairo_rectangle(cr, 0, 0, physical_width,
		scale_physical(ui->scale, rect.height));
	cairo_clip(cr);
	ui_draw(ui, part, cr, physical_width);
	cairo_restore(cr);
}

static bool draw(cairo_t *cr, const struct ui_content *content,
	const struct preview *preview) {
	struct color color = content->config->background.color;
	cairo_set_source_rgb(
		cr, color.red / 255.0, color.green / 255.0, color.blue / 255.0);
	cairo_paint(cr);
	struct ui *ui = ui_create(content, preview->scale);
	if (!ui) {
		return false;
	}
	// the logical size a compositor would configure for this output
	int32_t width = (int32_t)(((int64_t)preview->width * SCALE_DENOMINATOR +
					  preview->scale / 2) /
				  preview->scale);
	int32_t height =
		(int32_t)(((int64_t)preview->height * SCALE_DENOMINATOR +
				  preview->scale / 2) /
			  preview->scale);
	for (int i = 0; i < UI_PARTS; i++) {
		draw_part(ui, (enum ui_part)i, cr, width, height);
	}
	ui_destroy(ui);
	return cairo_status(cr) == CAIRO_STATUS_SUCCESS;
}

int app_preview(const struct config *config, const struct preview *preview) {
	struct clock clock = {.fd = -1};
	struct ui_content content;
	ui_content_init(&content, config, &clock);
	cairo_surface_t *surface = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, preview->width, preview->height);
	cairo_t *cr = cairo_create(surface);
	bool drawn = draw(cr, &content, preview);
	cairo_destroy(cr);
	cairo_status_t status =
		drawn ? cairo_surface_write_to_png(surface, preview->path)
		      : CAIRO_STATUS_SUCCESS;
	cairo_surface_destroy(surface);
	clock_stop(&clock);
	if (!drawn) {
		log_error("failed to draw the preview");
		return EXIT_FAILURE;
	}
	if (status != CAIRO_STATUS_SUCCESS) {
		log_error("failed to write the preview to %s: %s",
			preview->path, cairo_status_to_string(status));
		return EXIT_FAILURE;
	}
	log_info("wrote a %dx%d preview at scale %.3f to %s", preview->width,
		preview->height, scale_factor(preview->scale), preview->path);
	return EXIT_SUCCESS;
}
