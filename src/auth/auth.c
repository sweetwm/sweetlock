#include "auth/auth.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <sys/wait.h>
#include <unistd.h>

#include "auth/helper.h"
#include "auth/message.h"
#include "input/password.h"
#include "log.h"

extern char **environ;

// posix_spawn only execs, so this is safe once threads exist; the link
// keeps naming the running binary after an upgrade replaces the file
static bool spawn(struct auth *auth) {
	int fds[2];
	if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, fds) != 0) {
		log_error("failed to create the PAM helper socket: %m");
		return false;
	}
	char *const argv[] = {
		"sweetlock", HELPER_FLAG, (char *)auth->login, NULL};
	posix_spawn_file_actions_t actions;
	int error = posix_spawn_file_actions_init(&actions);
	if (error == 0) {
		error = posix_spawn_file_actions_adddup2(
			&actions, fds[1], HELPER_FD);
		pid_t pid = -1;
		if (error == 0) {
			error = posix_spawn(&pid, "/proc/self/exe", &actions,
				NULL, argv, environ);
		}
		posix_spawn_file_actions_destroy(&actions);
		auth->pid = error == 0 ? pid : -1;
	}
	close(fds[1]);
	if (error != 0) {
		errno = error;
		log_error("failed to start the PAM helper: %m");
		close(fds[0]);
		return false;
	}
	auth->fd = fds[0];
	if (fcntl(auth->fd, F_SETFL, O_NONBLOCK) != 0) {
		log_error("failed to set up the PAM helper socket: %m");
		return false;
	}
	return true;
}

// never waits on PAM: the helper may be sitting in a fail delay
static void stop_helper(struct auth *auth) {
	if (auth->fd >= 0) {
		close(auth->fd);
	}
	if (auth->pid > 0) {
		kill(auth->pid, SIGKILL);
		while (waitpid(auth->pid, NULL, 0) < 0 && errno == EINTR) {
		}
	}
	auth->fd = -1;
	auth->pid = -1;
}

static void end_attempt(struct auth *auth) {
	auth->busy = false;
	auth->cancellable = false;
	timerfd_settime(auth->timer_fd, 0, &(struct itimerspec){0}, NULL);
}

static bool restart(struct auth *auth) {
	stop_helper(auth);
	end_attempt(auth);
	if (!spawn(auth)) {
		stop_helper(auth);
		log_error("no PAM helper; exiting without unlocking");
		auth->lost = true;
		return false;
	}
	return true;
}

static void set_notice(
	struct auth *auth, enum auth_notice notice, const char *text) {
	auth->notice = notice;
	snprintf(auth->notice_text, sizeof auth->notice_text, "%s", text);
}

bool auth_start(struct auth *auth, const char *login) {
	*auth = (struct auth){.pid = -1, .fd = -1, .timer_fd = -1};
	if (!login[0]) {
		log_error("failed to look up the current user");
		return false;
	}
	auth->login = login;
	auth->timer_fd =
		timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
	if (auth->timer_fd < 0) {
		log_error("failed to create the attempt timer: %m");
		return false;
	}
	return spawn(auth);
}

bool auth_submit(struct auth *auth, const struct password *password) {
	size_t size = password->length + 1;
	if (send(auth->fd, password->bytes, size, MSG_NOSIGNAL) !=
		(ssize_t)size) {
		log_error("failed to send the password to the PAM helper: %m");
		return false;
	}
	auth->busy = true;
	auth->notice = AUTH_NOTICE_NONE;
	auth->notice_text[0] = '\0';
	// without the timer the attempt only cannot be cancelled
	const struct itimerspec spec = {
		.it_value = {.tv_sec = AUTH_CANCEL_AFTER_S}};
	if (timerfd_settime(auth->timer_fd, 0, &spec, NULL) != 0) {
		log_warn("failed to time the attempt: %m");
	}
	return true;
}

static void keep_notice(struct auth *auth, const struct message_reply *reply) {
	auth->notice = reply->kind == MESSAGE_INFO ? AUTH_NOTICE_INFO
						   : AUTH_NOTICE_ERROR;
	memcpy(auth->notice_text, reply->text, reply->length);
	auth->notice_text[reply->length] = '\0';
}

// an attempt in flight fails with the helper that ran it
static enum auth_result replace(struct auth *auth) {
	if (auth->respawns >= AUTH_RESPAWN_MAX) {
		log_error("the PAM helper exited; exiting without unlocking");
		auth->lost = true;
		return AUTH_LOST;
	}
	auth->respawns++;
	log_warn("the PAM helper exited; starting a new one (%d of %d)",
		auth->respawns, AUTH_RESPAWN_MAX);
	bool busy = auth->busy;
	if (!restart(auth)) {
		return AUTH_LOST;
	}
	if (busy) {
		set_notice(auth, AUTH_NOTICE_ERROR,
			"Authentication stopped; try again");
	}
	return AUTH_NOTICE;
}

// a verdict counts only as the answer to a password that is out, so a stray
// one can never unlock
enum auth_result auth_read(struct auth *auth) {
	enum auth_result result = AUTH_PENDING;
	for (;;) {
		char packet[MESSAGE_REPLY_MAX];
		ssize_t size = recv(auth->fd, packet, sizeof packet, MSG_TRUNC);
		if (size < 0 && errno == EINTR) {
			continue;
		}
		if (size < 0 && errno == EAGAIN) {
			return result;
		}
		if (size <= 0) {
			return replace(auth);
		}
		// the helper warns about its setup before any attempt
		struct message_reply reply;
		bool valid = message_reply(packet, (size_t)size, &reply);
		if (valid && (reply.kind == MESSAGE_INFO ||
				     reply.kind == MESSAGE_ERROR)) {
			keep_notice(auth, &reply);
			result = AUTH_NOTICE;
			continue;
		}
		if (!auth->busy) {
			log_error("the PAM helper replied with no password "
				  "out; ignoring it");
			continue;
		}
		// a broken reply still ends the attempt, or keys would stay
		// ignored
		if (!valid) {
			log_error("the PAM helper sent a malformed reply");
		}
		end_attempt(auth);
		bool success = valid && reply.kind == MESSAGE_SUCCESS;
		log_info("authentication %s", success ? "succeeded" : "failed");
		return success ? AUTH_SUCCESS : AUTH_FAILURE;
	}
}

bool auth_expire(struct auth *auth) {
	uint64_t expirations = 0;
	if (read(auth->timer_fd, &expirations, sizeof expirations) !=
			(ssize_t)sizeof expirations ||
		!auth->busy) {
		return false;
	}
	log_info("the attempt has run for %d s; Escape cancels it",
		AUTH_CANCEL_AFTER_S);
	auth->cancellable = true;
	return true;
}

// a fresh helper rather than a signal to the old one, which may be stuck in
// a module that ignores everything
bool auth_cancel(struct auth *auth) {
	if (!auth->cancellable) {
		return false;
	}
	log_info("cancelling the attempt");
	if (restart(auth)) {
		set_notice(auth, AUTH_NOTICE_INFO, "Cancelled");
	}
	return true;
}

void auth_stop(struct auth *auth) {
	stop_helper(auth);
	if (auth->timer_fd >= 0) {
		close(auth->timer_fd);
	}
	*auth = (struct auth){.pid = -1, .fd = -1, .timer_fd = -1};
}
