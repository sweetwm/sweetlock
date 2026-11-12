#ifndef SWEETLOCK_UI_UI_H
#define SWEETLOCK_UI_UI_H

#include <cairo.h>
#include <pango/pango.h>
#include <stdbool.h>
#include <stdint.h>

#include "input/input.h"
#include "input/layout.h"
#include "ui/avatar.h"

struct auth;
struct clock;
struct config;
struct field;
struct user;
struct worker;

// each part is its own subsurface, so a clock tick never redraws the field
enum ui_part {
	UI_CLOCK,
	UI_LOGIN,
	// the keyboard layout's badge in the top right corner
	UI_LAYOUT,
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
	const struct input *input;
	const struct auth *auth;
	const struct field *field;
	// NULL loads in the foreground, as the preview does
	struct worker *worker;
	const struct user *user;
	// plain text about the setup, shown while PAM has nothing to say
	const char *notice;
	struct avatar avatar;
	// xkeyboard-config's layout list for the badge; NULL guesses
	char *layouts;
	size_t layouts_length;
};

// enough to fill the field; more shows as an ellipsis
enum { UI_DOTS_MAX = 64 };

// one output's shaped text and rasters, at that output's scale
struct ui {
	const struct ui_content *content;
	uint32_t scale;
	PangoLayout *time;
	PangoLayout *date;
	PangoLayout *name;
	PangoLayout *entry;
	PangoLayout *status;
	PangoLayout *badge;
	// the layout name the badge's label was found for
	char badge_name[INPUT_LAYOUT_SIZE];
	char badge_label[LAYOUT_LABEL_SIZE];
	// the avatar cut to its circle at this scale, made on first use
	cairo_surface_t *avatar;
	// a bullet per code point, three bytes each
	char dots[UI_DOTS_MAX * 3 + 1];
};

void ui_content_start(struct ui_content *content, struct clock *clock);

// once per lock, on the worker when content has one
void ui_content_load_avatar(struct ui_content *content);

// after the worker has stopped
void ui_content_finish(struct ui_content *content);

bool ui_shows(const struct ui_content *content, enum ui_part part);

// a part that exists but is unmapped for now
bool ui_hidden(const struct ui_content *content, enum ui_part part);

bool ui_place(const struct ui_content *content, enum ui_part part,
	int32_t width, int32_t height, uint32_t scale, struct ui_rect *rect);

// the first call loads fontconfig, the slowest step in the program
struct ui *ui_create(const struct ui_content *content, uint32_t scale);

void ui_set_scale(struct ui *ui, uint32_t scale);

// rect is where ui_place put the part
bool ui_draw(struct ui *ui, enum ui_part part, cairo_t *cr,
	const struct ui_rect *rect);

// whether a point in the login part, in logical pixels, is on the reveal
// toggle
bool ui_hits_eye(
	const struct ui *ui, const struct ui_rect *rect, double x, double y);

void ui_destroy(struct ui *ui);

#endif
