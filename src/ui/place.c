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

int32_t ui_clock_inset(const struct config *config) {
	return config->clock.enabled
		       ? 0
		       : ui_theme.date_centre - ui_theme.date_size;
}

int32_t ui_name_centre(const struct config *config) {
	int32_t top = config->avatar.enabled
			      ? ui_theme.avatar_size + ui_theme.login_gap
			      : 0;
	return top + ui_theme.name_size / 2;
}

static int32_t field_top(const struct config *config) {
	if (config->username.enabled) {
		return ui_name_centre(config) + ui_theme.name_size / 2 +
		       ui_theme.login_gap;
	}
	return config->avatar.enabled
		       ? ui_theme.avatar_size + ui_theme.login_gap
		       : 0;
}

static int32_t clock_height(const struct config *config) {
	if (config->date.enabled) {
		return ui_theme.clock_height - ui_clock_inset(config);
	}
	return ui_theme.time_height;
}

static int32_t clock_text_top(const struct config *config) {
	return config->clock.enabled
		       ? ui_theme.time_centre - ui_theme.time_size / 2
		       : ui_theme.date_size / 2;
}

static void place_clock_and_login(const struct config *config, int32_t height,
	int32_t step, int32_t login_body, int32_t clock_box, int32_t *clock,
	int32_t *login) {
	bool field_only = !config->avatar.enabled && !config->username.enabled;
	int32_t gap =
		field_only ? ui_theme.clock_field_gap : ui_theme.clock_gap;
	int32_t spare = height - clock_box - login_body;
	if (spare / 3 < gap) {
		gap = spare > 0 ? spare / 3 : 0;
	}
	gap = snap(gap, step);
	if (!field_only) {
		// keep the login centred by moving the clock up first
		int32_t latest = *login - clock_box - gap;
		if (*clock > latest) {
			*clock = latest > 0 ? latest : 0;
			*login = *clock + clock_box + gap;
		}
		return;
	}
	*clock = snap((spare - gap - clock_text_top(config)) / 2, step);
	*login = *clock + clock_box + gap;
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
	// the status line has buffer space below the field so it never moves
	// the column
	int32_t login_body = field_top(config) + ui_theme.field_height;
	int32_t login = snap((height - login_body) / 2, step);
	int32_t clock = snap(ui_theme.clock_top + ui_clock_inset(config), step);
	int32_t clock_box = round_up(clock_height(config), step);
	if (ui_shows(content, UI_CLOCK)) {
		place_clock_and_login(config, height, step, login_body,
			clock_box, &clock, &login);
	}
	*rect = (struct ui_rect){
		.x = snap((width - w) / 2, step),
		.y = part == UI_CLOCK ? clock : login,
		.width = w,
		.height = part == UI_CLOCK
				  ? clock_box
				  : round_up(login_body + ui_theme.status_space,
					    step),
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
		.y = field_top(config),
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
