#include "auth/auth.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include "auth/helper.h"
#include "auth/message.h"
#include "input/password.h"
#include "log.h"

bool auth_start(struct auth *auth, const char *login) {
	*auth = (struct auth){.pid = -1, .fd = -1};
	if (!login[0]) {
		log_error("failed to look up the current user");
		return false;
	}

	int fds[2];
	if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, fds) != 0) {
		log_error("failed to create the PAM helper socket: %m");
		return false;
	}
	pid_t parent = getpid();
	pid_t pid = fork();
	if (pid == 0) {
		close(fds[0]);
		helper_run(fds[1], parent, login);
	}
	if (pid < 0) {
		log_error("failed to start the PAM helper: %m");
		close(fds[0]);
		close(fds[1]);
		return false;
	}
	close(fds[1]);
	*auth = (struct auth){.pid = pid, .fd = fds[0]};
	if (fcntl(auth->fd, F_SETFL, O_NONBLOCK) != 0) {
		log_error("failed to set up the PAM helper socket: %m");
		auth_stop(auth);
		return false;
	}
	return true;
}

bool auth_submit(struct auth *auth, const struct password *password) {
	size_t size = password->length + 1;
	if (send(auth->fd, password->bytes, size, MSG_NOSIGNAL) !=
		(ssize_t)size) {
		log_error("failed to send the password to the PAM helper: %m");
		return false;
	}
	auth->busy = true;
	return true;
}

enum auth_result auth_read(struct auth *auth) {
	char reply[MESSAGE_REPLY_SIZE];
	ssize_t size = recv(auth->fd, reply, sizeof reply, MSG_TRUNC);
	if (size < 0 && (errno == EAGAIN || errno == EINTR)) {
		return AUTH_PENDING;
	}
	if (size <= 0) {
		log_error("the PAM helper exited; exiting without unlocking");
		return AUTH_LOST;
	}
	auth->busy = false;
	bool success = false;
	if (!message_reply(reply, (size_t)size, &success)) {
		log_error("the PAM helper sent a malformed reply");
	}
	log_info("authentication %s", success ? "succeeded" : "failed");
	return success ? AUTH_SUCCESS : AUTH_FAILURE;
}

void auth_stop(struct auth *auth) {
	if (auth->fd >= 0) {
		close(auth->fd);
	}
	// the helper may be sitting in a PAM fail delay
	if (auth->pid > 0) {
		kill(auth->pid, SIGKILL);
		while (waitpid(auth->pid, NULL, 0) < 0 && errno == EINTR) {
		}
	}
	*auth = (struct auth){.pid = -1, .fd = -1};
}
