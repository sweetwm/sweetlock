#ifndef SWEETLOCK_UI_UI_H
#define SWEETLOCK_UI_UI_H

#include <cairo.h>
#include <pango/pango.h>
#include <stdbool.h>
#include <stdint.h>

#include "ui/user.h"

struct clock;
struct config;

// each part is its own subsurface, so a clock tick never redraws the field
enum ui_part {
	UI_CLOCK,
	UI_LOGIN,
	UI_PARTS,
};

// logical pixels on the output
struct ui_rect {
	int32_t x;
	int32_t y;
	int32_t width;
	int32_t height;
};

// what every output shows
struct ui_content {
	const struct config *config;
	const struct clock *clock;
	char name[USER_NAME_SIZE];
};

// one output's shaped text, at that output's scale
struct ui {
	const struct ui_content *content;
	uint32_t scale;
	PangoLayout *time;
	PangoLayout *date;
	PangoLayout *name;
};

// starts the clock and looks up the name; a failure leaves that text empty
void ui_content_init(struct ui_content *content, const struct config *config,
	struct clock *clock);

bool ui_shows(const struct ui_content *content, enum ui_part part);

bool ui_place(enum ui_part part, int32_t width, int32_t height, uint32_t scale,
	struct ui_rect *rect);

// the first call loads fontconfig, the slowest step in the program
struct ui *ui_create(const struct ui_content *content, uint32_t scale);

void ui_set_scale(struct ui *ui, uint32_t scale);

bool ui_draw(
	const struct ui *ui, enum ui_part part, cairo_t *cr, int32_t width);

void ui_destroy(struct ui *ui);

#endif
