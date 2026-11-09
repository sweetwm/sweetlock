#include "ui/ui.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "auth/auth.h"
#include "config/config.h"
#include "input/input.h"
#include "input/password.h"
#include "log.h"
#include "render/icon.h"
#include "render/text.h"
#include "ui/clock.h"
#include "ui/field.h"
#include "ui/user.h"
#include "wayland/scale.h"

// logical pixels, placed like default.png; login offsets are from the top
// of the login part
static const struct {
	int32_t width;
	int32_t clock_top;
	int32_t clock_height;
	int32_t time_centre;
	int32_t date_centre;
	int32_t login_above;
	int32_t login_height;
	int32_t avatar_top;
	int32_t avatar_size;
	int32_t name_centre;
	int32_t name_width;
	int32_t field_top;
	int32_t field_width;
	int32_t field_height;
	int32_t field_radius;
	int32_t field_padding;
	int32_t eye_inset;
	int32_t eye_hit;
	int32_t status_centre;
	double time_size;
	double date_size;
	double name_size;
	double entry_size;
	double status_size;
	double outline;
} theme = {
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
	.time_size = 88,
	.date_size = 18,
	.name_size = 28,
	.entry_size = 17,
	.status_size = 15,
	.outline = 1.5,
};

void ui_content_start(struct ui_content *content, struct clock *clock) {
	const struct config *config = content->config;
	content->clock = clock;
	const char *time = config->clock.enabled ? config->clock.format : NULL;
	const char *date = config->date.enabled ? config->date.format : NULL;
	if ((time || date) && !clock_start(clock, time, date)) {
		log_warn("showing no clock");
	}
	if (config->avatar.enabled) {
		avatar_load(&content->avatar, config->avatar.path,
			content->user->login, content->worker);
	}
}

void ui_content_finish(struct ui_content *content) {
	avatar_destroy(&content->avatar);
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
	if (width <= 0 || height <= 0 || width > SCALE_SIDE_MAX ||
		height > SCALE_SIDE_MAX) {
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
	// on a short output the login part moves down rather than cover the
	// date
	int32_t clock_bottom = snap(theme.clock_top, step) +
			       round_up(theme.clock_height, step);
	if (part == UI_LOGIN && y < clock_bottom) {
		y = clock_bottom;
	}
	int32_t h = part == UI_CLOCK ? theme.clock_height : theme.login_height;
	*rect = (struct ui_rect){
		.x = snap((width - w) / 2, step),
		.y = snap(y, step),
		.width = w,
		.height = round_up(h, step),
	};
	return true;
}

// the text's room in the field, between the padding and the eye
static int32_t entry_width(const struct config *config) {
	int32_t right = config->input.reveal_toggle ? theme.eye_hit
						    : theme.field_padding;
	return theme.field_width - theme.field_padding - right;
}

static void set_fonts(struct ui *ui) {
	const struct config *config = ui->content->config;
	const char *font = config->theme.font;
	double s = scale_factor(ui->scale);
	text_set_font(ui->time, font, PANGO_WEIGHT_BOLD, theme.time_size * s,
		theme.width * s);
	text_set_font(ui->date, font, PANGO_WEIGHT_SEMIBOLD,
		theme.date_size * s, theme.width * s);
	text_set_font(ui->name, font, PANGO_WEIGHT_NORMAL, theme.name_size * s,
		theme.name_width * s);
	text_set_font(ui->entry, font, PANGO_WEIGHT_NORMAL,
		theme.entry_size * s, entry_width(config) * s);
	// PAM's lines run longer than the field
	text_set_font(ui->status, font, PANGO_WEIGHT_NORMAL,
		theme.status_size * s, theme.width * s);
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
	};
	if (!ui->time || !ui->date || !ui->name || !ui->entry || !ui->status) {
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

static void set_source(cairo_t *cr, struct color color) {
	cairo_set_source_rgba(cr, color.red / 255.0, color.green / 255.0,
		color.blue / 255.0, color.alpha / 255.0);
}

static void draw_line(const struct ui *ui, cairo_t *cr, PangoLayout *layout,
	int32_t width, int32_t centre) {
	text_draw_centred(cr, layout, width, centre * scale_factor(ui->scale));
}

static void draw_clock(const struct ui *ui, cairo_t *cr, int32_t width) {
	const struct config *config = ui->content->config;
	const struct clock *clock = ui->content->clock;
	set_source(cr, config->theme.muted);
	if (config->clock.enabled) {
		text_set(ui->time, clock->time);
		draw_line(ui, cr, ui->time, width, theme.time_centre);
	}
	if (config->date.enabled) {
		text_set(ui->date, clock->date);
		draw_line(ui, cr, ui->date, width, theme.date_centre);
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
	int32_t size = scale_physical(ui->scale, theme.avatar_size);
	int32_t x = (width - size) / 2;
	int32_t y = scale_physical(ui->scale, theme.avatar_top);
	double radius = size / 2.0;
	set_source(cr, content->config->theme.field);
	cairo_arc(cr, x + radius, y + radius, radius, 0, 2 * M_PI);
	cairo_fill(cr);
	if (avatar->state == AVATAR_FALLBACK) {
		set_source(cr, content->config->theme.foreground);
		icon_person(cr, x + radius, y + radius, radius);
	} else if (avatar->state == AVATAR_IMAGE) {
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

// logical, inside the login part
static struct ui_rect field_rect(const struct ui_rect *part) {
	int32_t width = part->width < theme.field_width ? part->width
							: theme.field_width;
	return (struct ui_rect){
		.x = (part->width - width) / 2,
		.y = theme.field_top,
		.width = width,
		.height = theme.field_height,
	};
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
		text = "Checking\xe2\x80\xa6";
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
	struct ui_rect f = field_rect(part);
	double left = scale_physical(scale, f.x);
	double top = scale_physical(scale, f.y);
	double right = scale_physical(scale, f.x + f.width);
	double bottom = scale_physical(scale, f.y + f.height);
	double radius = theme.field_radius * s;
	enum field_state state = field_state(content->field,
		content->input->password->length, content->auth->busy);

	rounded(cr, left, top, right, bottom, radius);
	set_source(cr, config->theme.field);
	cairo_fill(cr);
	if (state == FIELD_FAILED) {
		// whole pixels, inset by half, so the line stays sharp
		double line = fmax(1, round(theme.outline * s));
		double in = line / 2;
		rounded(cr, left + in, top + in, right - in, bottom - in,
			radius - in);
		set_source(cr, config->theme.error);
		cairo_set_line_width(cr, line);
		cairo_stroke(cr);
	}
	set_source(cr, set_entry(ui, state));
	text_draw(cr, ui->entry, left + theme.field_padding * s,
		(top + bottom) / 2);
	if (config->input.reveal_toggle) {
		set_source(cr, config->theme.muted);
		icon_eye(cr, right - theme.eye_inset * s, (top + bottom) / 2, s,
			content->input->revealed);
	}
}

// PAM's text is plain text from the helper, never markup
static void draw_status(struct ui *ui, cairo_t *cr, int32_t width) {
	const struct config *config = ui->content->config;
	const struct auth *auth = ui->content->auth;
	const char *notice = ui->content->notice;
	if (auth->notice != AUTH_NOTICE_NONE) {
		set_source(cr, auth->notice == AUTH_NOTICE_ERROR
				       ? config->theme.error
				       : config->theme.muted);
		text_set(ui->status, auth->notice_text);
	} else if (notice && notice[0]) {
		set_source(cr, config->theme.error);
		text_set(ui->status, notice);
	} else {
		return;
	}
	draw_line(ui, cr, ui->status, width, theme.status_centre);
}

static bool draw_login(
	struct ui *ui, cairo_t *cr, const struct ui_rect *rect, int32_t width) {
	const struct config *config = ui->content->config;
	if (config->avatar.enabled && !draw_avatar(ui, cr, width)) {
		return false;
	}
	if (config->username.enabled) {
		set_source(cr, config->theme.foreground);
		draw_line(ui, cr, ui->name, width, theme.name_centre);
	}
	draw_field(ui, cr, rect);
	draw_status(ui, cr, width);
	return true;
}

bool ui_draw(struct ui *ui, enum ui_part part, cairo_t *cr,
	const struct ui_rect *rect) {
	int32_t width = scale_physical(ui->scale, rect->width);
	if (part == UI_CLOCK) {
		draw_clock(ui, cr, width);
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

bool ui_hits_eye(
	const struct ui *ui, const struct ui_rect *rect, double x, double y) {
	if (!ui->content->config->input.reveal_toggle) {
		return false;
	}
	struct ui_rect f = field_rect(rect);
	return x >= f.x + f.width - theme.eye_hit && x < f.x + f.width &&
	       y >= f.y && y < f.y + f.height;
}

void ui_destroy(struct ui *ui) {
	if (!ui) {
		return;
	}
	PangoLayout *layouts[] = {
		ui->time, ui->date, ui->name, ui->entry, ui->status};
	for (size_t i = 0; i < sizeof layouts / sizeof layouts[0]; i++) {
		if (layouts[i]) {
			g_object_unref(layouts[i]);
		}
	}
	cairo_surface_destroy(ui->avatar);
	free(ui);
}
