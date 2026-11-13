#include "ui/ui.h"

#include <limits.h>
#include <stdlib.h>

#include "config/config.h"
#include "input/input.h"
#include "input/layout.h"
#include "log.h"
#include "render/text.h"
#include "ui/clock.h"
#include "ui/internal.h"
#include "ui/user.h"
#include "wayland/scale.h"

const struct ui_theme ui_theme = {
	.width = 560,
	.clock_top = 32,
	.clock_height = 144,
	.time_centre = 56,
	.date_centre = 118,
	.login_above = 200,
	.login_height = 352,
	.avatar_top = 5,
	.avatar_size = 150,
	.name_centre = 200,
	.name_width = 400,
	.field_top = 253,
	.field_width = 310,
	.field_height = 54,
	.field_radius = 8,
	.field_padding = 18,
	.eye_inset = 28,
	.eye_hit = 56,
	.status_centre = 334,
	.badge_size = 46,
	.badge_margin = 22,
	.time_size = 88,
	.date_size = 18,
	.name_size = 28,
	.entry_size = 17,
	.status_size = 15,
	.badge_text_size = 15,
	.outline = 1.5,
};

// read once, after locked, like the Compose table
static void load_layouts(struct ui_content *content) {
	char path[PATH_MAX];
	char error[256] = "";
	enum file_result result = layout_list_load(path, &content->layouts,
		&content->layouts_length, error, sizeof error);
	if (result == FILE_OK) {
		log_debug("layout codes from %s", path);
	} else if (result == FILE_MISSING) {
		log_warn("no xkeyboard-config rules/evdev.lst; the layout "
			 "badge shows the name's first letters");
	} else {
		log_warn("%s: %s; the layout badge shows the name's first "
			 "letters",
			path, error);
	}
}

void ui_content_start(struct ui_content *content, struct clock *clock) {
	const struct config *config = content->config;
	content->clock = clock;
	const char *time = config->clock.enabled ? config->clock.format : NULL;
	const char *date = config->date.enabled ? config->date.format : NULL;
	if ((time || date) && !clock_start(clock, time, date)) {
		log_warn("showing no clock");
	}
	if (config->input.layout == LAYOUT_CORNER) {
		load_layouts(content);
	}
}

void ui_content_load_avatar(struct ui_content *content) {
	const struct config *config = content->config;
	if (config->avatar.enabled) {
		avatar_load(&content->avatar, config->avatar.path,
			content->user->login, content->worker);
	}
}

void ui_content_finish(struct ui_content *content) {
	avatar_destroy(&content->avatar);
	free(content->layouts);
	content->layouts = NULL;
}

// the text's room in the field, between the padding and the eye
static int32_t entry_width(const struct config *config) {
	int32_t right = config->input.reveal_toggle ? ui_theme.eye_hit
						    : ui_theme.field_padding;
	return ui_theme.field_width - ui_theme.field_padding - right;
}

static void set_fonts(struct ui *ui) {
	const struct config *config = ui->content->config;
	const char *font = config->theme.font;
	const char *clock_font =
		config->theme.clock_font[0] ? config->theme.clock_font : font;
	double s = scale_factor(ui->scale);
	text_set_font(ui->time, clock_font, PANGO_WEIGHT_BOLD,
		ui_theme.time_size * s, ui_theme.width * s);
	ui->digits = (struct text_digits){0};
	text_set_font(ui->date, clock_font, PANGO_WEIGHT_SEMIBOLD,
		ui_theme.date_size * s, ui_theme.width * s);
	text_set_font(ui->name, font, PANGO_WEIGHT_NORMAL,
		ui_theme.name_size * s, ui_theme.name_width * s);
	text_set_font(ui->entry, font, PANGO_WEIGHT_NORMAL,
		ui_theme.entry_size * s, entry_width(config) * s);
	// PAM's lines run longer than the field
	text_set_font(ui->status, font, PANGO_WEIGHT_NORMAL,
		ui_theme.status_size * s, ui_theme.width * s);
	text_set_font(ui->badge, font, PANGO_WEIGHT_BOLD,
		ui_theme.badge_text_size * s, ui_theme.badge_size * s);
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
		.entry = text_layout_create(),
		.status = text_layout_create(),
		.badge = text_layout_create(),
	};
	if (!ui->time || !ui->date || !ui->name || !ui->entry || !ui->status ||
		!ui->badge) {
		log_error("failed to create the UI's text layouts");
		ui_destroy(ui);
		return NULL;
	}
	set_fonts(ui);
	text_set(ui->name, content->user->name);
	return ui;
}

void ui_set_scale(struct ui *ui, uint32_t scale) {
	if (scale != ui->scale) {
		ui->scale = scale;
		set_fonts(ui);
		cairo_surface_destroy(ui->avatar);
		ui->avatar = NULL;
	}
}

void ui_destroy(struct ui *ui) {
	if (!ui) {
		return;
	}
	PangoLayout *layouts[] = {
		ui->time, ui->date, ui->name, ui->entry, ui->status, ui->badge};
	for (size_t i = 0; i < sizeof layouts / sizeof layouts[0]; i++) {
		if (layouts[i]) {
			g_object_unref(layouts[i]);
		}
	}
	cairo_surface_destroy(ui->avatar);
	free(ui);
}
