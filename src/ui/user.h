#ifndef SWEETLOCK_UI_USER_H
#define SWEETLOCK_UI_USER_H

enum { USER_NAME_SIZE = 256 };

struct user {
	// what the UI shows
	char name[USER_NAME_SIZE];
	// the account's login, empty when the lookup failed
	char login[USER_NAME_SIZE];
};

// main thread only: getpwuid shares one buffer
void user_lookup(struct user *user, const char *configured);

#endif
