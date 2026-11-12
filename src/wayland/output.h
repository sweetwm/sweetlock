#ifndef SWEETLOCK_WAYLAND_OUTPUT_H
#define SWEETLOCK_WAYLAND_OUTPUT_H

#include <stdbool.h>
#include <stdint.h>
#include <wayland-util.h>

#include "ui/ui.h"

struct backdrop;
struct display;
struct panel;
struct wl_output;
struct wl_surface;

enum { OUTPUT_NAME_SIZE = 64 };

struct output {
	struct wl_list link;
	struct display *display;
	struct wl_output *wl;
	// the registry name, matched against global_remove
	uint32_t global;
	char name[OUTPUT_NAME_SIZE];
	struct wl_surface *surface;
	struct wp_viewport *viewport;
	struct wp_fractional_scale_v1 *fractional;
	struct ext_session_lock_surface_v1 *lock_surface;
	struct zwlr_output_power_v1 *power;
	// its last power mode, ours or the compositor's
	bool power_off;
	bool mapped;
	int32_t width;
	int32_t height;
	uint32_t fractional_scale;
	int32_t integer_scale;
	int32_t pending_integer_scale;
	struct backdrop *backdrop;
	struct backdrop *shown;
	struct ui *ui;
	struct panel *panels[UI_PARTS];
	// the size or scale changed since the panels were placed
	bool relayout;
	// a broken UI is dropped once and the output stays locked without it
	bool ui_failed;
	bool ui_chosen;
};

// takes ownership of wl, released on failure
struct output *output_create(
	struct display *display, struct wl_output *wl, uint32_t global);

// creates this output's lock surface on the display's lock
bool output_lock(struct output *output);

void output_show_background(
	struct output *output, const struct ui_content *content);

// creates, places and draws the UI once mapped; true when it committed
bool output_render(
	struct output *output, const struct ui_content *content, bool shows_ui);

void output_mark_dirty(struct output *output, enum ui_part part);

bool output_hits_eye(const struct output *output,
	const struct wl_surface *surface, double x, double y);

void output_destroy(struct output *output);

#endif
