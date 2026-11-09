#include "auth/auth.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
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
	auth->notice = AUTH_NOTICE_NONE;
	auth->notice_text[0] = '\0';
	return true;
}

static void keep_notice(struct auth *auth, const struct message_reply *reply) {
	auth->notice = reply->kind == MESSAGE_KIND_INFO ? AUTH_NOTICE_INFO
							: AUTH_NOTICE_ERROR;
	memcpy(auth->notice_text, reply->text, reply->length);
	auth->notice_text[reply->length] = '\0';
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
			log_error("the PAM helper exited; exiting without "
				  "unlocking");
			return AUTH_LOST;
		}
		// the helper warns about its setup before any attempt
		struct message_reply reply;
		bool valid = message_reply(packet, (size_t)size, &reply);
		if (valid && (reply.kind == MESSAGE_KIND_INFO ||
				     reply.kind == MESSAGE_KIND_ERROR)) {
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
		auth->busy = false;
		bool success = valid && reply.kind == MESSAGE_KIND_SUCCESS;
		log_info("authentication %s", success ? "succeeded" : "failed");
		return success ? AUTH_SUCCESS : AUTH_FAILURE;
	}
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
