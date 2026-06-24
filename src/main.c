#include "ext-session-lock-v1-client-protocol.h"
#include "locky/lock.h"
#include "locky/locky.h"
#include "locky/log.h"
#include "locky/output.h"
#include "locky/registry.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <unistd.h>

#define DEFAULT_BACKGROUND 0x1E1E2E

static int signal_fds[2] = {-1, -1};

static void handle_signal(int sig) {
	(void)sig;
	if (signal_fds[1] >= 0) {
		ssize_t n = write(signal_fds[1], "x", 1);
		(void)n; // best-effort wake; nothing useful to do on failure
	}
}

static bool install_signal_handlers(void) {
	if (pipe(signal_fds) < 0) {
		return false;
	}
	fcntl(signal_fds[0], F_SETFL, O_NONBLOCK);
	fcntl(signal_fds[1], F_SETFL, O_NONBLOCK);

	struct sigaction sa = {0};
	sa.sa_handler = handle_signal;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	return true;
}

static void create_lock_surfaces(struct locky_state *state) {
	struct locky_output *output;
	wl_list_for_each (output, &state->outputs, link) {
		locky_lock_surface_create(output);
	}
}

static void destroy_outputs(struct locky_state *state) {
	struct locky_output *output, *tmp;
	wl_list_for_each_safe (output, tmp, &state->outputs, link) {
		locky_output_destroy(output);
	}
}

static bool run_event_loop(struct locky_state *state) {
	struct pollfd fds[2] = {
		{.fd = wl_display_get_fd(state->display), .events = POLLIN},
		{.fd = signal_fds[0], .events = POLLIN},
	};

	state->running = true;
	while (state->running) {
		while (wl_display_prepare_read(state->display) != 0) {
			if (wl_display_dispatch_pending(state->display) < 0) {
				locky_log(
					LOCKY_ERROR, "dispatch_pending failed");
				return false;
			}
		}

		if (wl_display_flush(state->display) < 0 && errno != EAGAIN) {
			wl_display_cancel_read(state->display);
			locky_log(LOCKY_ERROR, "wl_display_flush failed");
			return false;
		}

		if (poll(fds, 2, -1) < 0) {
			wl_display_cancel_read(state->display);
			if (errno == EINTR) {
				continue;
			}
			locky_log(LOCKY_ERROR, "poll failed");
			return false;
		}

		if (fds[0].revents & POLLIN) {
			if (wl_display_read_events(state->display) < 0) {
				locky_log(LOCKY_ERROR, "read_events failed");
				return false;
			}
		} else {
			wl_display_cancel_read(state->display);
		}

		if (fds[1].revents & POLLIN) {
			char buf[16];
			while (read(signal_fds[0], buf, sizeof(buf)) > 0) {
				// drain the pipe
			}
			locky_log(LOCKY_INFO, "signal received, shutting down");
			state->running = false;
		}

		if (wl_display_dispatch_pending(state->display) < 0) {
			locky_log(LOCKY_ERROR, "dispatch_pending failed");
			return false;
		}
	}
	return true;
}

int main(void) {
	locky_log_init(LOCKY_INFO);

	struct locky_state state = {0};
	state.background = DEFAULT_BACKGROUND;
	wl_list_init(&state.outputs);

	state.display = wl_display_connect(NULL);
	if (state.display == NULL) {
		locky_log(LOCKY_ERROR, "failed to connect to wayland display");
		return 1;
	}

	locky_registry_init(&state);
	if (wl_display_roundtrip(state.display) < 0) {
		locky_log(LOCKY_ERROR, "initial roundtrip failed");
		goto disconnect;
	}

	if (state.compositor == NULL || state.shm == NULL ||
		state.lock_manager == NULL) {
		locky_log(
			LOCKY_ERROR, "compositor is missing required globals");
		goto cleanup;
	}

	if (!locky_lock_acquire(&state)) {
		goto cleanup;
	}
	create_lock_surfaces(&state);

	if (!install_signal_handlers()) {
		locky_log(LOCKY_ERROR, "failed to install signal handlers");
		goto release;
	}

	run_event_loop(&state);

release:
	locky_lock_release(&state);
	wl_display_roundtrip(state.display);

cleanup:
	destroy_outputs(&state);
	if (state.lock_manager != NULL) {
		ext_session_lock_manager_v1_destroy(state.lock_manager);
	}
	if (state.compositor != NULL) {
		wl_compositor_destroy(state.compositor);
	}
	if (state.shm != NULL) {
		wl_shm_destroy(state.shm);
	}
	if (state.registry != NULL) {
		wl_registry_destroy(state.registry);
	}

disconnect:
	if (signal_fds[0] >= 0) {
		close(signal_fds[0]);
	}
	if (signal_fds[1] >= 0) {
		close(signal_fds[1]);
	}
	wl_display_disconnect(state.display);

	return state.finished ? 1 : 0;
}
