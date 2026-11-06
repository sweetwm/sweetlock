#ifndef SWEETLOCK_AUTH_AUTH_H
#define SWEETLOCK_AUTH_AUTH_H

#include <stdbool.h>
#include <sys/types.h>

struct password;

struct auth {
	pid_t pid;
	int fd;
	// a password is out and its reply has not arrived
	bool busy;
};

enum auth_result {
	AUTH_PENDING,
	AUTH_SUCCESS,
	AUTH_FAILURE,
	// the helper is gone and nothing can unlock any more
	AUTH_LOST,
};

// forks the PAM helper; must run before any library starts a thread
bool auth_start(struct auth *auth);

// the caller wipes the password once this returns
bool auth_submit(struct auth *auth, const struct password *password);

// reads a reply when fd is readable, logging the result
enum auth_result auth_read(struct auth *auth);

// never waits on PAM: the helper is killed, then reaped
void auth_stop(struct auth *auth);

#endif
