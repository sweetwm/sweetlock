#include "locky/registry.h"
#include "ext-session-lock-v1-client-protocol.h"
#include "locky/output.h"
#include <string.h>

static void handle_global(void *data, struct wl_registry *registry,
	uint32_t name, const char *interface, uint32_t version) {
	struct locky_state *state = data;

	if (strcmp(interface, wl_compositor_interface.name) == 0) {
		uint32_t bind_version = version < 4 ? version : 4;
		state->compositor = wl_registry_bind(
			registry, name, &wl_compositor_interface, bind_version);
	} else if (strcmp(interface, wl_shm_interface.name) == 0) {
		state->shm =
			wl_registry_bind(registry, name, &wl_shm_interface, 1);
	} else if (strcmp(interface,
			   ext_session_lock_manager_v1_interface.name) == 0) {
		state->lock_manager = wl_registry_bind(registry, name,
			&ext_session_lock_manager_v1_interface, 1);
	} else if (strcmp(interface, wl_output_interface.name) == 0) {
		locky_output_create(state, registry, name, version);
	}
}

static void handle_global_remove(
	void *data, struct wl_registry *registry, uint32_t name) {
	(void)registry;
	struct locky_state *state = data;
	struct locky_output *output, *tmp;
	wl_list_for_each_safe (output, tmp, &state->outputs, link) {
		if (output->global_name == name) {
			locky_output_destroy(output);
			break;
		}
	}
}

static const struct wl_registry_listener registry_listener = {
	.global = handle_global,
	.global_remove = handle_global_remove,
};

void locky_registry_init(struct locky_state *state) {
	state->registry = wl_display_get_registry(state->display);
	wl_registry_add_listener(state->registry, &registry_listener, state);
}
