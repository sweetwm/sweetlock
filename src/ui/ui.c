#include "ui/ui.h"

#include <stdlib.h>

#include "config.h"
#include "log.h"
#include "render/text.h"
#include "scale.h"
#include "ui/clock.h"

// logical pixels, placed like default.png
static const struct {
	int32_t width;
	int32_t clock_top;
	int32_t clock_height;
	int32_t time_centre;
	int32_t date_centre;
	// the login part starts this far above the output's centre
	int32_t login_above;
	int32_t login_height;
	int32_t name_centre;
	int32_t name_width;
	double time_size;
	double date_size;
	double name_size;
} theme = {
	.width = 560,
	.clock_top = 32,
	.clock_height = 144,
	.time_centre = 56,
	.date_centre = 118,
	.login_above = 200,
	.login_height = 320,
	.name_centre = 200,
	.name_width = 400,
	.time_size = 88,
	.date_size = 18,
	.name_size = 28,
};

enum { UI_OUTPUT_MAX = 1 << 16 };

void ui_content_init(struct ui_content *content, const struct config *config,
	struct clock *clock) {
	*content = (struct ui_content){.config = config, .clock = clock};
	const char *time = config->clock.enabled ? config->clock.format : NULL;
	const char *date = config->date.enabled ? config->date.format : NULL;
	if ((time || date) && !clock_start(clock, time, date)) {
		log_warn("showing no clock");
	}
	user_name(content->name, config->username.text);
}

bool ui_shows(const struct ui_content *content, enum ui_part part) {
	const struct config *config = content->config;
	if (part == UI_CLOCK) {
		return config->clock.enabled || config->date.enabled;
	}
	return true;
}

static int32_t snap(int32_t value, int32_t step) {
	return value < 0 ? 0 : (value + step / 2) / step * step;
}

static int32_t round_up(int32_t value, int32_t step) {
	return (value + step - 1) / step * step;
}

bool ui_place(enum ui_part part, int32_t width, int32_t height, uint32_t scale,
	struct ui_rect *rect) {
	if (width <= 0 || height <= 0 || width > UI_OUTPUT_MAX ||
		height > UI_OUTPUT_MAX) {
		return false;
	}
	int32_t step = scale_step(scale);
	int32_t w = width < theme.width ? width : theme.width;
	w = w / step * step;
	if (w == 0) {
		w = step;
	}
	int32_t y = part == UI_CLOCK ? theme.clock_top
				     : height / 2 - theme.login_above;
	int32_t h = part == UI_CLOCK ? theme.clock_height : theme.login_height;
	*rect = (struct ui_rect){
		.x = snap((width - w) / 2, step),
		.y = snap(y, step),
		.width = w,
		.height = round_up(h, step),
	};
	return true;
}

static void set_fonts(struct ui *ui) {
	const char *font = ui->content->config->theme.font;
	double s = scale_factor(ui->scale);
	text_set_font(ui->time, font, PANGO_WEIGHT_BOLD, theme.time_size * s,
		theme.width * s);
	text_set_font(ui->date, font, PANGO_WEIGHT_SEMIBOLD,
		theme.date_size * s, theme.width * s);
	text_set_font(ui->name, font, PANGO_WEIGHT_NORMAL, theme.name_size * s,
		theme.name_width * s);
}

struct ui *ui_create(const struct ui_content *content, uint32_t scale) {
	struct ui *ui = calloc(1, sizeof *ui);
	if (!ui) {
		log_error("out of memory allocating the UI");
		return NULL;
	}
	*ui = (struct ui){
		.content = content,
		.scale = scale,
		.time = text_layout_create(),
		.date = text_layout_create(),
		.name = text_layout_create(),
	};
	if (!ui->time || !ui->date || !ui->name) {
		log_error("failed to create the UI's text layouts");
		ui_destroy(ui);
		return NULL;
	}
	set_fonts(ui);
	text_set(ui->name, content->name);
	return ui;
}

void ui_set_scale(struct ui *ui, uint32_t scale) {
	if (scale != ui->scale) {
		ui->scale = scale;
		set_fonts(ui);
	}
}

static void set_source(cairo_t *cr, struct color color) {
	cairo_set_source_rgba(cr, color.red / 255.0, color.green / 255.0,
		color.blue / 255.0, color.alpha / 255.0);
}

static void draw_line(const struct ui *ui, cairo_t *cr, PangoLayout *layout,
	int32_t width, int32_t centre) {
	text_draw_centred(cr, layout, width, centre * scale_factor(ui->scale));
}

bool ui_draw(
	const struct ui *ui, enum ui_part part, cairo_t *cr, int32_t width) {
	const struct config *config = ui->content->config;
	const struct clock *clock = ui->content->clock;
	if (part == UI_CLOCK) {
		set_source(cr, config->theme.muted);
		if (config->clock.enabled) {
			text_set(ui->time, clock->time);
			draw_line(ui, cr, ui->time, width, theme.time_centre);
		}
		if (config->date.enabled) {
			text_set(ui->date, clock->date);
			draw_line(ui, cr, ui->date, width, theme.date_centre);
		}
	} else if (config->username.enabled) {
		set_source(cr, config->theme.foreground);
		draw_line(ui, cr, ui->name, width, theme.name_centre);
	}
	cairo_status_t status = cairo_status(cr);
	if (status != CAIRO_STATUS_SUCCESS) {
		log_error("failed to draw the UI: %s",
			cairo_status_to_string(status));
		return false;
	}
	return true;
}

void ui_destroy(struct ui *ui) {
	if (!ui) {
		return;
	}
	if (ui->time) {
		g_object_unref(ui->time);
	}
	if (ui->date) {
		g_object_unref(ui->date);
	}
	if (ui->name) {
		g_object_unref(ui->name);
	}
	free(ui);
}
