#ifndef SWEETLOCK_AUTH_AUTH_H
#define SWEETLOCK_AUTH_AUTH_H

#include <stdbool.h>
#include <sys/types.h>

#include "auth/message.h"

struct password;

enum auth_notice {
	AUTH_NOTICE_NONE,
	AUTH_NOTICE_INFO,
	AUTH_NOTICE_ERROR,
};

struct auth {
	pid_t pid;
	int fd;
	// a password is out and its reply has not arrived
	bool busy;
	// PAM's last text in this attempt, kept until the next one starts
	enum auth_notice notice;
	char notice_text[MESSAGE_TEXT_MAX + 1];
};

enum auth_result {
	AUTH_PENDING,
	// still pending, with a new notice
	AUTH_NOTICE,
	AUTH_SUCCESS,
	AUTH_FAILURE,
	// the helper is gone and nothing can unlock any more
	AUTH_LOST,
};

// forks the PAM helper; must run before any library starts a thread, and
// fails on an empty login
bool auth_start(struct auth *auth, const char *login);

// the caller wipes the password once this returns
bool auth_submit(struct auth *auth, const struct password *password);

// reads every packet waiting when fd is readable, logging the result
enum auth_result auth_read(struct auth *auth);

// never waits on PAM: the helper is killed, then reaped
void auth_stop(struct auth *auth);

#endif
