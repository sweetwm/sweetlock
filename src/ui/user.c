#include "ui/user.h"

#include <pwd.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include "utf8.h"

static bool take(char name[USER_NAME_SIZE], const char *text, size_t length) {
	if (length == 0 || length >= USER_NAME_SIZE ||
		!utf8_is_plain(text, length)) {
		return false;
	}
	memcpy(name, text, length);
	name[length] = '\0';
	return true;
}

void user_lookup(struct user *user, const char *configured) {
	*user = (struct user){0};
	const struct passwd *account = getpwuid(getuid());
	if (account) {
		take(user->login, account->pw_name, strlen(account->pw_name));
	}
	if (take(user->name, configured, strlen(configured)) || !account) {
		return;
	}
	// the GECOS fields after the full name are office and phone numbers
	const char *gecos = account->pw_gecos ? account->pw_gecos : "";
	if (!take(user->name, gecos, strcspn(gecos, ","))) {
		memcpy(user->name, user->login, sizeof user->name);
	}
}
