#include "app/signals.h"

#include <signal.h>
#ifdef __FreeBSD__
#include <sys/event.h>
#include <sys/procctl.h>
#include <time.h>
#else
#include <sys/prctl.h>
#include <sys/signalfd.h>
#include <unistd.h>
#endif

#include "app/app.h"
#include "log.h"

static const int watched[] = {SIGHUP, SIGTERM, SIGINT, SIGUSR1};

enum { WATCHED_COUNT = sizeof watched / sizeof watched[0] };

static void signal_set(sigset_t *set) {
	sigemptyset(set);
	for (int i = 0; i < WATCHED_COUNT; i++) {
		sigaddset(set, watched[i]);
	}
}

// no ptrace and no core dump, so the password stays in this process
static bool undumpable(void) {
#ifdef __FreeBSD__
	int trace = PROC_TRACE_CTL_DISABLE;
	return procctl(P_PID, 0, PROC_TRACE_CTL, &trace) == 0;
#else
	return prctl(PR_SET_DUMPABLE, 0, 0, 0, 0) == 0;
#endif
}

bool app_protect(void) {
	if (!undumpable()) {
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

#ifdef __FreeBSD__
bool signals_open(int *fd) {
	struct kevent events[WATCHED_COUNT];
	for (int i = 0; i < WATCHED_COUNT; i++) {
		EV_SET(&events[i], watched[i], EVFILT_SIGNAL, EV_ADD, 0, 0,
			NULL);
	}
	*fd = kqueuex(KQUEUE_CLOEXEC);
	if (*fd < 0 || kevent(*fd, events, WATCHED_COUNT, NULL, 0, NULL) != 0) {
		log_error("failed to monitor signals: %m");
		return false;
	}
	return true;
}
#else
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
#endif

// SIGTERM and SIGINT exit and leave the session locked, as the protocol
// requires, SIGHUP only means the terminal that started us is gone
static enum signal_action handle(int number, enum signal_action action) {
	if (number == SIGUSR1) {
		log_info("received SIGUSR1; unlocking");
		return action == SIGNAL_NONE ? SIGNAL_UNLOCK : action;
	}
	if (number == SIGHUP) {
		log_info("received SIGHUP; staying locked");
		return action;
	}
	log_info("received %s; exiting without unlocking",
		number == SIGTERM ? "SIGTERM" : "SIGINT");
	return SIGNAL_EXIT;
}

#ifdef __FreeBSD__
enum signal_action signals_read(int fd) {
	enum signal_action action = SIGNAL_NONE;
	const struct timespec now = {0};
	struct kevent event;
	while (kevent(fd, NULL, 0, &event, 1, &now) == 1) {
		int number = (int)event.ident;
		// kqueue only notes it; a blocked signal stays pending
		sigset_t one;
		sigemptyset(&one);
		sigaddset(&one, number);
		(void)sigtimedwait(&one, NULL, &now);
		action = handle(number, action);
	}
	return action;
}
#else
enum signal_action signals_read(int fd) {
	enum signal_action action = SIGNAL_NONE;
	struct signalfd_siginfo info;
	while (read(fd, &info, sizeof info) == (ssize_t)sizeof info) {
		action = handle((int)info.ssi_signo, action);
	}
	return action;
}
#endif
