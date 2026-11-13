#include "ui/ui.h"

#include <math.h>
#include <string.h>

#include "auth/auth.h"
#include "config/config.h"
#include "input/input.h"
#include "input/layout.h"
#include "input/password.h"
#include "log.h"
#include "render/icon.h"
#include "render/text.h"
#include "ui/clock.h"
#include "ui/field.h"
#include "ui/internal.h"
#include "ui/status.h"
#include "wayland/scale.h"

static void set_source(cairo_t *cr, struct color color) {
	cairo_set_source_rgba(cr, color.red / 255.0, color.green / 255.0,
		color.blue / 255.0, color.alpha / 255.0);
}

static void draw_line(const struct ui *ui, cairo_t *cr, PangoLayout *layout,
	int32_t width, int32_t centre) {
	text_draw_centred(cr, layout, width, centre * scale_factor(ui->scale));
}

static void draw_clock(struct ui *ui, cairo_t *cr, int32_t width) {
	const struct config *config = ui->content->config;
	const struct clock *clock = ui->content->clock;
	set_source(cr, config->theme.muted);
	if (config->clock.enabled) {
		if (!ui->digits.widest) {
			text_measure_digits(ui->time, &ui->digits);
		}
		text_set(ui->time, clock->time);
		text_draw_digits_centred(cr, ui->time, &ui->digits, width,
			ui_theme.time_centre * scale_factor(ui->scale));
	}
	if (config->date.enabled) {
		text_set(ui->date, clock->date);
		draw_line(ui, cr, ui->date, width, ui_theme.date_centre);
	}
}

// scaled once per output scale, so a key press only copies it
static cairo_surface_t *round_avatar(cairo_surface_t *image, int32_t size) {
	cairo_surface_t *out =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size, size);
	cairo_t *cr = cairo_create(out);
	cairo_arc(cr, size / 2.0, size / 2.0, size / 2.0, 0, 2 * M_PI);
	cairo_clip(cr);
	double factor = (double)size / cairo_image_surface_get_width(image);
	cairo_scale(cr, factor, factor);
	cairo_set_source_surface(cr, image, 0, 0);
	cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_GOOD);
	cairo_paint(cr);
	cairo_status_t status = cairo_status(cr);
	cairo_destroy(cr);
	if (status != CAIRO_STATUS_SUCCESS) {
		log_error("failed to scale the avatar: %s",
			cairo_status_to_string(status));
		cairo_surface_destroy(out);
		return NULL;
	}
	return out;
}

// the disc stays behind a picture, so a transparent one still has it
static bool draw_avatar(struct ui *ui, cairo_t *cr, int32_t width) {
	const struct ui_content *content = ui->content;
	const struct avatar *avatar = &content->avatar;
	int32_t size = scale_physical(ui->scale, ui_theme.avatar_size);
	int32_t x = (width - size) / 2;
	int32_t y = scale_physical(ui->scale, ui_theme.avatar_top);
	double radius = size / 2.0;
	set_source(cr, content->config->theme.field);
	cairo_arc(cr, x + radius, y + radius, radius, 0, 2 * M_PI);
	cairo_fill(cr);
	// the worker writes image, so read it only once the job is back
	if (avatar->loaded && !avatar->image) {
		set_source(cr, content->config->theme.foreground);
		icon_person(cr, x + radius, y + radius, radius);
	} else if (avatar->loaded) {
		if (!ui->avatar) {
			ui->avatar = round_avatar(avatar->image, size);
		}
		if (!ui->avatar) {
			return false;
		}
		cairo_set_source_surface(cr, ui->avatar, x, y);
		cairo_paint(cr);
	}
	return true;
}

static void rounded(cairo_t *cr, double left, double top, double right,
	double bottom, double radius) {
	cairo_new_sub_path(cr);
	cairo_arc(cr, right - radius, top + radius, radius, -M_PI / 2, 0);
	cairo_arc(cr, right - radius, bottom - radius, radius, 0, M_PI / 2);
	cairo_arc(cr, left + radius, bottom - radius, radius, M_PI / 2, M_PI);
	cairo_arc(cr, left + radius, top + radius, radius, M_PI, 1.5 * M_PI);
	cairo_close_path(cr);
}

static const char *dots(struct ui *ui, const struct password *password) {
	size_t count = 0;
	for (size_t i = 0; i < password->length && count < UI_DOTS_MAX; i++) {
		// a U+2022 bullet per UTF-8 lead byte
		count += ((unsigned char)password->bytes[i] & 0xc0) != 0x80;
	}
	for (size_t i = 0; i < count; i++) {
		memcpy(ui->dots + 3 * i, "\xe2\x80\xa2", 3);
	}
	ui->dots[3 * count] = '\0';
	return ui->dots;
}

// the revealed password is set as plain text, never as markup
static struct color set_entry(struct ui *ui, enum field_state state) {
	const struct config *config = ui->content->config;
	const struct input *input = ui->content->input;
	const char *text = config->input.placeholder;
	struct color color = config->theme.foreground;
	PangoEllipsizeMode mode = PANGO_ELLIPSIZE_END;
	switch (state) {
	case FIELD_IDLE:
		break;
	case FIELD_TYPING:
		text = input->revealed ? input->password->bytes
				       : dots(ui, input->password);
		// typing happens at the end, so the start gives way
		mode = PANGO_ELLIPSIZE_START;
		break;
	case FIELD_CHECKING:
		// PAM's prompt keeps the status line, so the hint goes here
		text = ui->content->auth->cancellable ? "Press Escape to cancel"
						      : "Checking\xe2\x80\xa6";
		color = config->theme.muted;
		break;
	case FIELD_FAILED:
		text = "Wrong password";
		color = config->theme.error;
		break;
	}
	pango_layout_set_ellipsize(ui->entry, mode);
	text_set(ui->entry, text);
	return color;
}

static void draw_field(struct ui *ui, cairo_t *cr, const struct ui_rect *part) {
	const struct ui_content *content = ui->content;
	const struct config *config = content->config;
	uint32_t scale = ui->scale;
	double s = scale_factor(scale);
	struct ui_rect f = ui_field_rect(config, part);
	double left = scale_physical(scale, f.x);
	double top = scale_physical(scale, f.y);
	double right = scale_physical(scale, f.x + f.width);
	double bottom = scale_physical(scale, f.y + f.height);
	double radius = ui_theme.field_radius * s;
	enum field_state state = field_state(content->field,
		content->input->password->length, content->auth->busy);

	rounded(cr, left, top, right, bottom, radius);
	set_source(cr, config->theme.field);
	cairo_fill(cr);
	if (state == FIELD_FAILED) {
		// whole pixels, inset by half, so the line stays sharp
		double line = fmax(1, round(ui_theme.outline * s));
		double in = line / 2;
		rounded(cr, left + in, top + in, right - in, bottom - in,
			radius - in);
		set_source(cr, config->theme.error);
		cairo_set_line_width(cr, line);
		cairo_stroke(cr);
	}
	set_source(cr, set_entry(ui, state));
	text_draw(cr, ui->entry, left + ui_theme.field_padding * s,
		(top + bottom) / 2);
	if (config->input.reveal_toggle) {
		set_source(cr, config->theme.muted);
		icon_eye(cr, right - ui_theme.eye_inset * s, (top + bottom) / 2,
			s, content->input->revealed);
	}
}

// PAM's text and the layout name are plain text, never markup
static void draw_status(struct ui *ui, cairo_t *cr, int32_t width) {
	const struct config *config = ui->content->config;
	const char *text = NULL;
	switch (status_line(ui->content, &text)) {
	case STATUS_NONE:
		return;
	case STATUS_PAM_INFO:
	case STATUS_LAYOUT:
		set_source(cr, config->theme.muted);
		break;
	case STATUS_CAPS_LOCK:
		set_source(cr, config->theme.foreground);
		break;
	case STATUS_PAM_ERROR:
	case STATUS_NOTICE:
		set_source(cr, config->theme.error);
		break;
	}
	text_set(ui->status, text);
	draw_line(ui, cr, ui->status, width,
		ui_theme.status_centre - ui_login_skip(config));
}

static bool draw_login(
	struct ui *ui, cairo_t *cr, const struct ui_rect *rect, int32_t width) {
	const struct config *config = ui->content->config;
	if (config->avatar.enabled && !draw_avatar(ui, cr, width)) {
		return false;
	}
	if (config->username.enabled) {
		set_source(cr, config->theme.foreground);
		draw_line(ui, cr, ui->name, width, ui_theme.name_centre);
	}
	draw_field(ui, cr, rect);
	draw_status(ui, cr, width);
	return true;
}

// one layout needs no badge, and the name is looked up only when it changes
static void draw_badge(struct ui *ui, cairo_t *cr, int32_t width) {
	const struct ui_content *content = ui->content;
	const char *name = content->input->status.layout;
	if (!name[0]) {
		return;
	}
	if (strcmp(name, ui->badge_name) != 0) {
		layout_label(content->layouts, content->layouts_length, name,
			ui->badge_label, sizeof ui->badge_label);
		memcpy(ui->badge_name, name, sizeof ui->badge_name);
		text_set(ui->badge, ui->badge_label);
	}
	if (!ui->badge_label[0]) {
		return;
	}
	double radius = scale_physical(ui->scale, ui_theme.badge_size) / 2.0;
	set_source(cr, content->config->theme.field);
	cairo_arc(cr, width / 2.0, radius, radius, 0, 2 * M_PI);
	cairo_fill(cr);
	set_source(cr, content->config->theme.foreground);
	text_draw_centred(cr, ui->badge, width, radius);
}

bool ui_draw(struct ui *ui, enum ui_part part, cairo_t *cr,
	const struct ui_rect *rect) {
	int32_t width = scale_physical(ui->scale, rect->width);
	if (part == UI_CLOCK) {
		draw_clock(ui, cr, width);
	} else if (part == UI_LAYOUT) {
		draw_badge(ui, cr, width);
	} else if (!draw_login(ui, cr, rect, width)) {
		return false;
	}
	cairo_status_t status = cairo_status(cr);
	if (status != CAIRO_STATUS_SUCCESS) {
		log_error("failed to draw the UI: %s",
			cairo_status_to_string(status));
		return false;
	}
	return true;
}
