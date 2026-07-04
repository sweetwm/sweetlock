#include "auth/auth.h"
#include "app/log.h"
#include "auth/comm.h"
#include "input/password.h"
#include "render/render.h"
#include <unistd.h>

bool locky_auth_init(void) {
	if (getuid() != geteuid() || getgid() != getegid()) {
		locky_log(LOCKY_ERROR,
			"locky must not be setuid when using the PAM backend");
		return false;
	}
	return locky_comm_spawn_child();
}

void locky_auth_submit(struct locky_state *state) {
	if (state->auth_state == LOCKY_AUTH_VALIDATING) {
		return; // a check is already in flight
	}
	state->auth_state = LOCKY_AUTH_VALIDATING;
	if (!locky_comm_write_request(&state->password)) {
		state->auth_state = LOCKY_AUTH_INVALID;
	}
	locky_render_all_outputs(state);
}

int locky_auth_fd(void) {
	return locky_comm_reply_fd();
}

void locky_auth_handle_reply(struct locky_state *state) {
	bool success = false;
	if (!locky_comm_read_reply(&success)) {
		locky_log(LOCKY_ERROR, "auth child terminated; staying locked");
		state->running = false;
		return;
	}

	if (success) {
		locky_log(LOCKY_INFO, "authentication succeeded");
		state->unlock = true;
		state->running = false;
	} else {
		locky_log(LOCKY_INFO, "authentication failed");
		state->auth_state = LOCKY_AUTH_INVALID;
		locky_password_clear(&state->password);
		locky_render_all_outputs(state);
	}
}

void locky_auth_finish(void) {
	locky_comm_finish();
}
