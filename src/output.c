#include "locky/output.h"
#include "ext-session-lock-v1-client-protocol.h"
#include "locky/lock.h"
#include <stdlib.h>

static void output_handle_geometry(void *data, struct wl_output *wl_output,
	int32_t x, int32_t y, int32_t physical_width, int32_t physical_height,
	int32_t subpixel, const char *make, const char *model,
	int32_t transform) {
	(void)data, (void)wl_output, (void)x, (void)y, (void)physical_width;
	(void)physical_height, (void)subpixel, (void)make, (void)model;
	(void)transform;
}

static void output_handle_mode(void *data, struct wl_output *wl_output,
	uint32_t flags, int32_t width, int32_t height, int32_t refresh) {
	(void)data, (void)wl_output, (void)flags, (void)width, (void)height;
	(void)refresh;
}

static void output_handle_done(void *data, struct wl_output *wl_output) {
	(void)wl_output;
	struct locky_output *output = data;
	// create the lock surface once the lock exists; covers output hotplug
	if (output->state->lock != NULL && output->lock_surface == NULL) {
		locky_lock_surface_create(output);
	}
}

static void output_handle_scale(
	void *data, struct wl_output *wl_output, int32_t factor) {
	(void)wl_output;
	struct locky_output *output = data;
	output->scale = factor;
}

static void output_handle_name(
	void *data, struct wl_output *wl_output, const char *name) {
	(void)data, (void)wl_output, (void)name;
}

static void output_handle_description(
	void *data, struct wl_output *wl_output, const char *description) {
	(void)data, (void)wl_output, (void)description;
}

static const struct wl_output_listener output_listener = {
	.geometry = output_handle_geometry,
	.mode = output_handle_mode,
	.done = output_handle_done,
	.scale = output_handle_scale,
	.name = output_handle_name,
	.description = output_handle_description,
};

void locky_output_create(struct locky_state *state,
	struct wl_registry *registry, uint32_t global_name, uint32_t version) {
	struct locky_output *output = calloc(1, sizeof(*output));
	if (output == NULL) {
		return;
	}
	output->state = state;
	output->global_name = global_name;
	output->scale = 1;

	uint32_t bind_version = version < 4 ? version : 4;
	output->wl_output = wl_registry_bind(
		registry, global_name, &wl_output_interface, bind_version);
	wl_output_add_listener(output->wl_output, &output_listener, output);
	wl_list_insert(&state->outputs, &output->link);
}

void locky_output_destroy(struct locky_output *output) {
	wl_list_remove(&output->link);
	if (output->lock_surface != NULL) {
		ext_session_lock_surface_v1_destroy(output->lock_surface);
	}
	if (output->surface != NULL) {
		wl_surface_destroy(output->surface);
	}
	if (wl_output_get_version(output->wl_output) >=
		WL_OUTPUT_RELEASE_SINCE_VERSION) {
		wl_output_release(output->wl_output);
	} else {
		wl_output_destroy(output->wl_output);
	}
	free(output);
}
