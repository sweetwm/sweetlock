#include "locky/comm.h"
#include "locky/log.h"
#include "locky/pam.h"
#include "locky/password.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define MAX_REQUEST_SIZE 4096

static int request_pipe[2] = {-1, -1};
static int reply_pipe[2] = {-1, -1};

static bool read_full(int fd, void *dst, size_t size) {
	char *buf = dst;
	size_t offset = 0;
	while (offset < size) {
		ssize_t n = read(fd, &buf[offset], size - offset);
		if (n < 0) {
			if (errno == EINTR) {
				continue;
			}
			return false;
		}
		if (n == 0) {
			return false; // unexpected EOF
		}
		offset += (size_t)n;
	}
	return true;
}

static bool write_full(int fd, const void *src, size_t size) {
	const char *buf = src;
	size_t offset = 0;
	while (offset < size) {
		ssize_t n = write(fd, &buf[offset], size - offset);
		if (n < 0) {
			if (errno == EINTR) {
				continue;
			}
			return false;
		}
		offset += (size_t)n;
	}
	return true;
}

bool locky_comm_spawn_child(void) {
	if (pipe(request_pipe) != 0 || pipe(reply_pipe) != 0) {
		locky_log(LOCKY_ERROR, "failed to create auth pipes");
		return false;
	}

	pid_t pid = fork();
	if (pid < 0) {
		locky_log(LOCKY_ERROR, "failed to fork auth child");
		return false;
	}
	if (pid == 0) {
		// child keeps the request-read and reply-write ends
		close(request_pipe[1]);
		close(reply_pipe[0]);
		locky_pam_run_child(); // never returns
	}

	// parent keeps the request-write and reply-read ends
	close(request_pipe[0]);
	request_pipe[0] = -1;
	close(reply_pipe[1]);
	reply_pipe[1] = -1;
	return true;
}

bool locky_comm_write_request(struct locky_password *pw) {
	size_t size = pw->len + 1; // include the trailing NUL
	bool ok = write_full(request_pipe[1], &size, sizeof(size)) &&
		  write_full(request_pipe[1], pw->buffer, size);
	if (!ok) {
		locky_log(LOCKY_ERROR, "failed to send password to auth child");
	}
	// clear the secret from this process as soon as it is handed off
	locky_password_clear(pw);
	return ok;
}

ssize_t locky_comm_read_request(char **out) {
	size_t size = 0;
	if (!read_full(request_pipe[0], &size, sizeof(size))) {
		return 0; // EOF or error: treat as clean shutdown signal
	}
	if (size == 0 || size > MAX_REQUEST_SIZE) {
		return -1;
	}

	char *buf = malloc(size);
	if (buf == NULL) {
		return -1;
	}
	mlock(buf, size); // best effort; keep the secret out of swap

	if (!read_full(request_pipe[0], buf, size) || buf[size - 1] != '\0') {
		locky_comm_free_request(buf, size);
		return -1;
	}
	*out = buf;
	return (ssize_t)size;
}

void locky_comm_free_request(char *buf, size_t size) {
	if (buf == NULL) {
		return;
	}
	explicit_bzero(buf, size);
	munlock(buf, size);
	free(buf);
}

bool locky_comm_write_reply(bool success) {
	return write_full(reply_pipe[1], &success, sizeof(success));
}

bool locky_comm_read_reply(bool *success) {
	return read_full(reply_pipe[0], success, sizeof(*success));
}

int locky_comm_reply_fd(void) {
	return reply_pipe[0];
}

void locky_comm_finish(void) {
	if (request_pipe[1] >= 0) {
		close(request_pipe[1]); // EOF tells the child to exit
		request_pipe[1] = -1;
	}
	if (reply_pipe[0] >= 0) {
		close(reply_pipe[0]);
		reply_pipe[0] = -1;
	}
}
