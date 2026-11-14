#include "app/signals.h"

#include <signal.h>
#include <sys/prctl.h>
#include <sys/signalfd.h>
#include <unistd.h>

#include "app/app.h"
#include "log.h"

static void signal_set(sigset_t *set) {
	sigemptyset(set);
	sigaddset(set, SIGHUP);
	sigaddset(set, SIGTERM);
	sigaddset(set, SIGINT);
	sigaddset(set, SIGUSR1);
}

bool app_protect(void) {
	if (prctl(PR_SET_DUMPABLE, 0, 0, 0, 0) != 0) {
		log_error("failed to make the process undumpable: %m");
		return false;
	}
	// stderr may be a pipe whose reader is gone; the helper inherits this
	struct sigaction ignore = {.sa_handler = SIG_IGN};
	if (sigaction(SIGPIPE, &ignore, NULL) != 0) {
		log_error("failed to ignore SIGPIPE: %m");
		return false;
	}
	sigset_t set;
	signal_set(&set);
	if (sigprocmask(SIG_BLOCK, &set, NULL) != 0) {
		log_error("failed to block signals: %m");
		return false;
	}
	return true;
}

bool signals_open(int *fd) {
	sigset_t set;
	signal_set(&set);
	*fd = signalfd(-1, &set, SFD_CLOEXEC | SFD_NONBLOCK);
	if (*fd < 0) {
		log_error("failed to monitor signals: %m");
		return false;
	}
	return true;
}

// SIGTERM and SIGINT exit and leave the session locked, as the protocol
// requires, SIGHUP only means the terminal that started us is gone
enum signal_action signals_read(int fd) {
	enum signal_action action = SIGNAL_NONE;
	struct signalfd_siginfo info;
	while (read(fd, &info, sizeof info) == (ssize_t)sizeof info) {
		if (info.ssi_signo == SIGUSR1) {
			log_info("received SIGUSR1; unlocking");
			if (action == SIGNAL_NONE) {
				action = SIGNAL_UNLOCK;
			}
		} else if (info.ssi_signo == SIGHUP) {
			log_info("received SIGHUP; staying locked");
		} else {
			log_info("received %s; exiting without unlocking",
				info.ssi_signo == SIGTERM ? "SIGTERM"
							  : "SIGINT");
			action = SIGNAL_EXIT;
		}
	}
	return action;
}
