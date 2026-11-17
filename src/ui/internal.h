#ifndef SWEETLOCK_UI_INTERNAL_H
#define SWEETLOCK_UI_INTERNAL_H

#include <stdint.h>

#include "ui/ui.h"

struct config;

// logical pixels
struct ui_theme {
	int32_t width;
	int32_t clock_top;
	int32_t clock_height;
	int32_t time_height;
	int32_t time_centre;
	int32_t date_centre;
	int32_t clock_gap;
	int32_t clock_field_gap;
	int32_t login_gap;
	int32_t avatar_size;
	int32_t name_width;
	int32_t field_width;
	int32_t field_height;
	int32_t field_radius;
	int32_t field_padding;
	int32_t eye_inset;
	int32_t eye_hit;
	int32_t status_offset;
	int32_t status_space;
	int32_t badge_size;
	int32_t badge_margin;
	int32_t time_size;
	int32_t date_size;
	int32_t name_size;
	double entry_size;
	double mask_size;
	double mask_spacing;
	double status_size;
	double badge_text_size;
	double outline;
};

extern const struct ui_theme ui_theme;

int32_t ui_clock_inset(const struct config *config);
int32_t ui_name_centre(const struct config *config);

// logical, inside the login part
struct ui_rect ui_field_rect(
	const struct config *config, const struct ui_rect *part);

#endif
