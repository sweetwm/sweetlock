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

enum {
	AUTH_CANCEL_AFTER_S = 10,
	AUTH_RESPAWN_MAX = 3,
};

struct auth {
	pid_t pid;
	int fd;
	int timer_fd;
	const char *login;
	int respawns;
	bool busy;
	bool cancellable;
	bool lost;
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

// spawns the PAM helper; fails on an empty login, which must outlive auth
bool auth_start(struct auth *auth, const char *login);

// the caller wipes the password once this returns
bool auth_submit(struct auth *auth, const struct password *password);

enum auth_result auth_read(struct auth *auth);

// timer_fd is readable; true when the attempt became cancellable
bool auth_expire(struct auth *auth);

bool auth_cancel(struct auth *auth);

// never waits on PAM: the helper is killed, then reaped
void auth_stop(struct auth *auth);

#endif
