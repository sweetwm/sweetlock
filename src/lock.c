#include "locky/lock.h"
#include "ext-session-lock-v1-client-protocol.h"
#include "locky/log.h"
#include "locky/render.h"
#include <stdlib.h>

static void lock_surface_handle_configure(void *data,
	struct ext_session_lock_surface_v1 *lock_surface, uint32_t serial,
	uint32_t width, uint32_t height) {
	struct locky_output *output = data;
	output->width = width;
	output->height = height;
	ext_session_lock_surface_v1_ack_configure(lock_surface, serial);
	locky_render_output(output);
}

static const struct ext_session_lock_surface_v1_listener lock_surface_listener =
	{
		.configure = lock_surface_handle_configure,
};

void locky_lock_surface_create(struct locky_output *output) {
	struct locky_state *state = output->state;

	output->surface = wl_compositor_create_surface(state->compositor);
	if (output->surface == NULL) {
		locky_log(LOCKY_ERROR, "failed to create wl_surface");
		return;
	}

	output->lock_surface = ext_session_lock_v1_get_lock_surface(
		state->lock, output->surface, output->wl_output);
	ext_session_lock_surface_v1_add_listener(
		output->lock_surface, &lock_surface_listener, output);
}

static void lock_handle_locked(void *data, struct ext_session_lock_v1 *lock) {
	(void)lock;
	struct locky_state *state = data;
	state->locked = true;
	locky_log(LOCKY_INFO, "session locked");
}

static void lock_handle_finished(void *data, struct ext_session_lock_v1 *lock) {
	(void)lock;
	struct locky_state *state = data;
	state->finished = true;
	state->running = false;
	locky_log(LOCKY_ERROR, "session lock finished without locking (another "
			       "locker running?)");
}

static const struct ext_session_lock_v1_listener lock_listener = {
	.locked = lock_handle_locked,
	.finished = lock_handle_finished,
};

bool locky_lock_acquire(struct locky_state *state) {
	state->lock = ext_session_lock_manager_v1_lock(state->lock_manager);
	if (state->lock == NULL) {
		locky_log(LOCKY_ERROR, "failed to acquire session lock");
		return false;
	}
	ext_session_lock_v1_add_listener(state->lock, &lock_listener, state);
	return true;
}

void locky_lock_release(struct locky_state *state) {
	if (state->lock == NULL) {
		return;
	}

	if (state->finished) {
		if (state->locked) {
			ext_session_lock_v1_unlock_and_destroy(state->lock);
		} else {
			ext_session_lock_v1_destroy(state->lock);
		}
	} else if (state->unlock && state->locked) {
		ext_session_lock_v1_unlock_and_destroy(state->lock);
	} else if (!state->locked) {
		ext_session_lock_v1_destroy(state->lock);
	} else {
		return;
	}

	state->lock = NULL;
}
