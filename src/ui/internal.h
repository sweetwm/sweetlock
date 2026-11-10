#ifndef SWEETLOCK_UI_INTERNAL_H
#define SWEETLOCK_UI_INTERNAL_H

#include <stdint.h>

#include "ui/ui.h"

struct config;

// logical pixels, placed like default.png; login offsets are from the top
// of the login part
struct ui_theme {
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
	int32_t badge_size;
	int32_t badge_margin;
	double time_size;
	double date_size;
	double name_size;
	double entry_size;
	double status_size;
	double badge_text_size;
	double outline;
};

extern const struct ui_theme ui_theme;

// what the login part leaves off its top, where the avatar and name go
int32_t ui_login_skip(const struct config *config);

// logical, inside the login part
struct ui_rect ui_field_rect(
	const struct config *config, const struct ui_rect *part);

#endif
