#include "wayland/display.h"

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <wayland-client.h>

#include "ext-session-lock-v1-client-protocol.h"
#include "log.h"
#include "viewporter-client-protocol.h"
#include "wayland/buffer.h"
#include "wayland/output.h"

// highest versions whose requests and events this code handles
enum {
	COMPOSITOR_VERSION = 4,
	SHM_VERSION = 1,
	OUTPUT_VERSION = 4,
	VIEWPORTER_VERSION = 1,
	LOCK_MANAGER_VERSION = 1,
};

static void *bind(struct wl_registry *registry, uint32_t name,
	const struct wl_interface *interface, uint32_t advertised,
	uint32_t supported) {
	return wl_registry_bind(registry, name, interface,
		advertised < supported ? advertised : supported);
}

// a new output while the lock is pending or held gets its lock surface at
// once
static void add_output(
	struct display *display, uint32_t name, uint32_t version) {
	struct wl_output *wl = bind(display->registry, name,
		&wl_output_interface, version, OUTPUT_VERSION);
	struct output *output = wl ? output_create(display, wl, name) : NULL;
	if (!output) {
		log_error("failed to bind an output");
		display->failed = true;
		return;
	}
	wl_list_insert(display->outputs.prev, &output->link);
	if (display->lock.wl && !output_lock(output)) {
		display->failed = true;
	}
}

static bool is(const char *interface, const struct wl_interface *wanted) {
	return strcmp(interface, wanted->name) == 0;
}

static void registry_global(void *data, struct wl_registry *registry,
	uint32_t name, const char *interface, uint32_t version) {
	struct display *display = data;

	// a second advertisement of a global we already hold is ignored
	if (!display->compositor && is(interface, &wl_compositor_interface)) {
		display->compositor = bind(registry, name,
			&wl_compositor_interface, version, COMPOSITOR_VERSION);
	} else if (!display->shm && is(interface, &wl_shm_interface)) {
		display->shm = bind(registry, name, &wl_shm_interface, version,
			SHM_VERSION);
	} else if (!display->viewporter &&
		   is(interface, &wp_viewporter_interface)) {
		display->viewporter = bind(registry, name,
			&wp_viewporter_interface, version, VIEWPORTER_VERSION);
	} else if (!display->lock_manager &&
		   is(interface, &ext_session_lock_manager_v1_interface)) {
		display->lock_manager = bind(registry, name,
			&ext_session_lock_manager_v1_interface, version,
			LOCK_MANAGER_VERSION);
	} else if (is(interface, &wl_output_interface)) {
		add_output(display, name, version);
	}
}

static void registry_global_remove(
	void *data, struct wl_registry *registry, uint32_t name) {
	(void)registry;
	struct display *display = data;
	struct output *output = NULL;
	wl_list_for_each(output, &display->outputs, link) {
		if (output->global == name) {
			log_info("removed output %s", output->name);
			wl_list_remove(&output->link);
			output_destroy(output);
			return;
		}
	}
}

static const struct wl_registry_listener registry_listener = {
	.global = registry_global,
	.global_remove = registry_global_remove,
};

static const char *missing_global(const struct display *display) {
	if (!display->compositor) {
		return wl_compositor_interface.name;
	}
	if (!display->shm) {
		return wl_shm_interface.name;
	}
	if (!display->viewporter) {
		return wp_viewporter_interface.name;
	}
	if (!display->lock_manager) {
		return ext_session_lock_manager_v1_interface.name;
	}
	return NULL;
}

bool display_connect(struct display *display, const struct config *config) {
	*display = (struct display){0};
	wl_list_init(&display->outputs);
	display->wl = wl_display_connect(NULL);
	if (!display->wl) {
		log_error("failed to connect to Wayland: %m");
		return false;
	}

	display->registry = wl_display_get_registry(display->wl);
	if (!display->registry || wl_registry_add_listener(display->registry,
					  &registry_listener, display) != 0) {
		log_error("failed to create the Wayland registry");
		return false;
	}
	if (wl_display_roundtrip(display->wl) < 0) {
		display_log_lost(display);
		return false;
	}

	const char *missing = missing_global(display);
	if (missing) {
		log_error(
			"the compositor does not offer the required global %s",
			missing);
		return false;
	}
	display->solid = buffer_solid(display->shm, config->background.color);
	return display->solid && !display->failed;
}

bool display_lock(struct display *display, const struct timespec *start) {
	if (!lock_request(&display->lock, display->lock_manager, start)) {
		return false;
	}
	struct output *output = NULL;
	wl_list_for_each(output, &display->outputs, link) {
		if (!output_lock(output)) {
			return false;
		}
	}
	return true;
}

void display_log_lost(struct display *display) {
	int error = wl_display_get_error(display->wl);
	if (error == EPROTO) {
		const struct wl_interface *interface = NULL;
		uint32_t id = 0;
		uint32_t code = wl_display_get_protocol_error(
			display->wl, &interface, &id);
		log_error("Wayland protocol error %u on %s@%u", code,
			interface ? interface->name : "unknown", id);
		return;
	}
	log_error(
		"Wayland connection lost: %s", strerror(error ? error : errno));
}

void display_disconnect(struct display *display) {
	if (!display->wl) {
		return;
	}
	lock_abandon(&display->lock);
	struct output *output = NULL;
	struct output *next = NULL;
	wl_list_for_each_safe(output, next, &display->outputs, link) {
		wl_list_remove(&output->link);
		output_destroy(output);
	}
	if (display->solid) {
		wl_buffer_destroy(display->solid);
	}
	if (display->lock_manager) {
		ext_session_lock_manager_v1_destroy(display->lock_manager);
	}
	if (display->viewporter) {
		wp_viewporter_destroy(display->viewporter);
	}
	if (display->shm) {
		wl_shm_destroy(display->shm);
	}
	if (display->compositor) {
		wl_compositor_destroy(display->compositor);
	}
	if (display->registry) {
		wl_registry_destroy(display->registry);
	}
	wl_display_disconnect(display->wl);
	*display = (struct display){0};
}
