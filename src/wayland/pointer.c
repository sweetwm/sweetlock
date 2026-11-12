#include "wayland/pointer.h"

#include <linux/input-event-codes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <wayland-client.h>

#include "cursor-shape-v1-client-protocol.h"
#include "input/input.h"
#include "log.h"
#include "wayland/display.h"

struct pointer {
	struct display *display;
	struct wl_pointer *wl;
	// NULL without wp_cursor_shape_manager_v1
	struct wp_cursor_shape_device_v1 *shape;
	struct wl_surface *surface;
	uint32_t serial;
	double x;
	double y;
	bool over_eye;
};

static bool over_eye(const struct pointer *pointer) {
	return pointer->surface &&
	       display_hits_eye(pointer->display, pointer->surface, pointer->x,
		       pointer->y);
}

static void set_shape(struct pointer *pointer, bool over) {
	pointer->over_eye = over;
	if (pointer->shape) {
		wp_cursor_shape_device_v1_set_shape(pointer->shape,
			pointer->serial,
			over ? WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_POINTER
			     : WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT);
	}
}

static void pointer_enter(void *data, struct wl_pointer *wl, uint32_t serial,
	struct wl_surface *surface, wl_fixed_t x, wl_fixed_t y) {
	(void)wl;
	struct pointer *pointer = data;
	pointer->surface = surface;
	pointer->serial = serial;
	pointer->x = wl_fixed_to_double(x);
	pointer->y = wl_fixed_to_double(y);
	set_shape(pointer, over_eye(pointer));
}

static void pointer_leave(void *data, struct wl_pointer *wl, uint32_t serial,
	struct wl_surface *surface) {
	(void)wl;
	(void)serial;
	(void)surface;
	struct pointer *pointer = data;
	pointer->surface = NULL;
	pointer->over_eye = false;
}

// the shape changes only on crossing the eye's edge
static void pointer_motion(void *data, struct wl_pointer *wl, uint32_t time,
	wl_fixed_t x, wl_fixed_t y) {
	(void)wl;
	(void)time;
	struct pointer *pointer = data;
	input_touch(pointer->display->input);
	pointer->x = wl_fixed_to_double(x);
	pointer->y = wl_fixed_to_double(y);
	bool over = over_eye(pointer);
	if (over != pointer->over_eye) {
		set_shape(pointer, over);
	}
}

static void pointer_button(void *data, struct wl_pointer *wl, uint32_t serial,
	uint32_t time, uint32_t button, uint32_t state) {
	(void)wl;
	(void)serial;
	(void)time;
	struct pointer *pointer = data;
	struct input *input = pointer->display->input;
	if (state != WL_POINTER_BUTTON_STATE_PRESSED) {
		return;
	}
	if (!input_wakes(input) && !input_show(input) && button == BTN_LEFT &&
		over_eye(pointer)) {
		input_toggle_reveal(input);
	}
}

static void pointer_axis(void *data, struct wl_pointer *wl, uint32_t time,
	uint32_t axis, wl_fixed_t value) {
	(void)wl;
	input_touch(((struct pointer *)data)->display->input);
	(void)time;
	(void)axis;
	(void)value;
}

static void pointer_frame(void *data, struct wl_pointer *wl) {
	(void)data;
	(void)wl;
}

static void pointer_axis_source(
	void *data, struct wl_pointer *wl, uint32_t source) {
	(void)data;
	(void)wl;
	(void)source;
}

static void pointer_axis_stop(
	void *data, struct wl_pointer *wl, uint32_t time, uint32_t axis) {
	(void)data;
	(void)wl;
	(void)time;
	(void)axis;
}

static void pointer_axis_discrete(
	void *data, struct wl_pointer *wl, uint32_t axis, int32_t discrete) {
	(void)data;
	(void)wl;
	(void)axis;
	(void)discrete;
}

// every event up to version 7, the highest wl_seat this code binds
static const struct wl_pointer_listener pointer_listener = {
	.enter = pointer_enter,
	.leave = pointer_leave,
	.motion = pointer_motion,
	.button = pointer_button,
	.axis = pointer_axis,
	.frame = pointer_frame,
	.axis_source = pointer_axis_source,
	.axis_stop = pointer_axis_stop,
	.axis_discrete = pointer_axis_discrete,
};

static void release(struct wl_pointer *wl) {
	if (wl_pointer_get_version(wl) >= WL_POINTER_RELEASE_SINCE_VERSION) {
		wl_pointer_release(wl);
	} else {
		wl_pointer_destroy(wl);
	}
}

struct pointer *pointer_create(struct display *display, struct wl_pointer *wl) {
	struct pointer *pointer = calloc(1, sizeof *pointer);
	if (!pointer) {
		release(wl);
		return NULL;
	}
	*pointer = (struct pointer){.display = display, .wl = wl};
	if (display->cursor_shape) {
		pointer->shape = wp_cursor_shape_manager_v1_get_pointer(
			display->cursor_shape, wl);
		// the compositor's cursor stays, which is fine
		if (!pointer->shape) {
			log_warn("failed to get a cursor shape device");
		}
	}
	wl_pointer_add_listener(wl, &pointer_listener, pointer);
	return pointer;
}

void pointer_destroy(struct pointer *pointer) {
	if (!pointer) {
		return;
	}
	if (pointer->shape) {
		wp_cursor_shape_device_v1_destroy(pointer->shape);
	}
	release(pointer->wl);
	free(pointer);
}
