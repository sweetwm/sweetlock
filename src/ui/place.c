#include "ui/ui.h"

#include "config/config.h"
#include "ui/internal.h"
#include "wayland/scale.h"

bool ui_shows(const struct ui_content *content, enum ui_part part) {
	const struct config *config = content->config;
	if (part == UI_CLOCK) {
		return config->clock.enabled || config->date.enabled;
	}
	if (part == UI_LAYOUT) {
		return config->input.layout == LAYOUT_CORNER;
	}
	return true;
}

bool ui_hidden(const struct ui_content *content, enum ui_part part) {
	return part == UI_LOGIN && content->input->quiet;
}

static int32_t snap(int32_t value, int32_t step) {
	return value < 0 ? 0 : (value + step / 2) / step * step;
}

static int32_t round_up(int32_t value, int32_t step) {
	return (value + step - 1) / step * step;
}

// with no avatar and no name, the clock takes their place above the field
static bool clock_centred(const struct config *config) {
	return !config->avatar.enabled && !config->username.enabled;
}

int32_t ui_login_skip(const struct config *config) {
	return clock_centred(config) ? ui_theme.field_top : 0;
}

static int32_t centred_lift(void) {
	int32_t clock_top = ui_theme.name_centre - ui_theme.date_centre +
			    ui_theme.time_centre -
			    (int32_t)ui_theme.time_size / 2;
	return (clock_top - ui_theme.avatar_top) / 2;
}

bool ui_place(const struct ui_content *content, enum ui_part part,
	int32_t width, int32_t height, uint32_t scale, struct ui_rect *rect) {
	if (width <= 0 || height <= 0 || width > SCALE_SIDE_MAX ||
		height > SCALE_SIDE_MAX) {
		return false;
	}
	const struct config *config = content->config;
	int32_t step = scale_step(scale);
	if (part == UI_LAYOUT) {
		int32_t side = round_up(ui_theme.badge_size, step);
		// snapped inwards, so the margin is never lost
		int32_t x = width - ui_theme.badge_margin - side;
		*rect = (struct ui_rect){
			.x = x < 0 ? 0 : x / step * step,
			.y = round_up(ui_theme.badge_margin, step),
			.width = side,
			.height = side,
		};
		return true;
	}
	int32_t w = width < ui_theme.width ? width : ui_theme.width;
	w = w / step * step;
	if (w == 0) {
		w = step;
	}
	int32_t login = height / 2 - ui_theme.login_above;
	int32_t clock = ui_theme.clock_top;
	if (clock_centred(config)) {
		login -= centred_lift();
		int32_t centred =
			login + ui_theme.name_centre - ui_theme.date_centre;
		clock = centred > clock ? centred : clock;
	}
	int32_t skip = ui_login_skip(config);
	login += skip;
	// on a short output the login part moves down rather than cover the
	// date
	int32_t clock_bottom =
		snap(clock, step) + round_up(ui_theme.clock_height, step);
	if (login < clock_bottom) {
		login = clock_bottom;
	}
	int32_t y = part == UI_CLOCK ? clock : login;
	int32_t h = part == UI_CLOCK ? ui_theme.clock_height
				     : ui_theme.login_height - skip;
	*rect = (struct ui_rect){
		.x = snap((width - w) / 2, step),
		.y = snap(y, step),
		.width = w,
		.height = round_up(h, step),
	};
	return true;
}

struct ui_rect ui_field_rect(
	const struct config *config, const struct ui_rect *part) {
	int32_t width = part->width < ui_theme.field_width
				? part->width
				: ui_theme.field_width;
	return (struct ui_rect){
		.x = (part->width - width) / 2,
		.y = ui_theme.field_top - ui_login_skip(config),
		.width = width,
		.height = ui_theme.field_height,
	};
}

bool ui_hits_eye(
	const struct ui *ui, const struct ui_rect *rect, double x, double y) {
	if (!ui->content->config->input.reveal_toggle) {
		return false;
	}
	struct ui_rect f = ui_field_rect(ui->content->config, rect);
	return x >= f.x + f.width - ui_theme.eye_hit && x < f.x + f.width &&
	       y >= f.y && y < f.y + f.height;
}
