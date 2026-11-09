#include "wayland/power.h"

#include <wayland-client.h>

#include "input/input.h"
#include "log.h"
#include "wayland/display.h"
#include "wayland/output.h"
#include "wlr-output-power-management-unstable-v1-client-protocol.h"

// the compositor reports every change, including another client's, so an
// output another tool turned off is woken by our key too
static void power_mode(
	void *data, struct zwlr_output_power_v1 *power, uint32_t mode) {
	(void)power;
	struct output *output = data;
	output->power_off = mode == ZWLR_OUTPUT_POWER_V1_MODE_OFF;
	power_update(output->display);
}

static void power_failed(void *data, struct zwlr_output_power_v1 *power) {
	(void)power;
	struct output *output = data;
	log_warn("output %s refused power control; it stays on", output->name);
	power_detach(output);
	power_update(output->display);
}

static const struct zwlr_output_power_v1_listener power_listener = {
	.mode = power_mode,
	.failed = power_failed,
};

bool power_start(struct display *display) {
	if (!display->power_manager) {
		log_warn(
			"the compositor does not offer %s; the screens stay on",
			zwlr_output_power_manager_v1_interface.name);
		return false;
	}
	display->power_started = true;
	struct output *output = NULL;
	wl_list_for_each(output, &display->outputs, link) {
		power_attach(output);
	}
	return true;
}

// a missing control only keeps this output on
void power_attach(struct output *output) {
	struct display *display = output->display;
	if (!display->power_started || output->power) {
		return;
	}
	output->power = zwlr_output_power_manager_v1_get_output_power(
		display->power_manager, output->wl);
	if (!output->power) {
		log_warn("failed to control the power of output %s",
			output->name);
		return;
	}
	zwlr_output_power_v1_add_listener(
		output->power, &power_listener, output);
}

void power_detach(struct output *output) {
	if (output->power) {
		zwlr_output_power_v1_destroy(output->power);
		output->power = NULL;
	}
	output->power_off = false;
}

void power_set(struct display *display, bool on) {
	struct output *output = NULL;
	wl_list_for_each(output, &display->outputs, link) {
		if (output->power && output->power_off == on) {
			zwlr_output_power_v1_set_mode(output->power,
				on ? ZWLR_OUTPUT_POWER_V1_MODE_ON
				   : ZWLR_OUTPUT_POWER_V1_MODE_OFF);
			output->power_off = !on;
		}
	}
	power_update(display);
}

void power_update(struct display *display) {
	bool off = false;
	const struct output *output = NULL;
	wl_list_for_each(output, &display->outputs, link) {
		off |= output->power_off;
	}
	display->input->screens_off = off;
}
