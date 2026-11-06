#include "app.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#include "log.h"

// one byte means the child holds the lock, EOF means it exited first
static _Noreturn void wait_locked(int fd) {
	// the parent only waits, so a caller may still stop it with SIGTERM
	sigset_t none;
	sigemptyset(&none);
	sigprocmask(SIG_SETMASK, &none, NULL);
	char byte = 0;
	ssize_t size = 0;
	do {
		size = read(fd, &byte, sizeof byte);
	} while (size < 0 && errno == EINTR);
	_exit(size == 1 ? EXIT_SUCCESS : EXIT_FAILURE);
}

// a caller reading stdout to EOF would otherwise wait until unlock; stderr
// stays for the log
static void detach(void) {
	if (setsid() < 0) {
		log_warn("failed to start a new session: %m");
	}
	int null = open("/dev/null", O_RDWR | O_CLOEXEC);
	if (null < 0 || dup2(null, STDIN_FILENO) < 0 ||
		dup2(null, STDOUT_FILENO) < 0) {
		log_warn("failed to point stdin and stdout at /dev/null: %m");
	}
	if (null > STDOUT_FILENO) {
		close(null);
	}
	log_info("running in the background as pid %d", (int)getpid());
}

int app_daemonize(void) {
	int fds[2];
	// a socket, not a pipe, so MSG_NOSIGNAL spares us SIGPIPE if the parent
	// was killed
	if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fds) != 0) {
		log_error("failed to set up -f: %m; locking in the foreground");
		return -1;
	}
	pid_t pid = fork();
	if (pid < 0) {
		log_error(
			"failed to fork for -f: %m; locking in the foreground");
		close(fds[0]);
		close(fds[1]);
		return -1;
	}
	if (pid > 0) {
		close(fds[1]);
		wait_locked(fds[0]);
	}
	close(fds[0]);
	detach();
	return fds[1];
}

void app_report_locked(int fd) {
	char byte = 1;
	if (send(fd, &byte, sizeof byte, MSG_NOSIGNAL) !=
		(ssize_t)sizeof byte) {
		log_warn("failed to tell the -f parent about the lock: %m");
	}
	close(fd);
}
