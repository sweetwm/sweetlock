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

void user_name(char name[USER_NAME_SIZE], const char *configured) {
	name[0] = '\0';
	if (take(name, configured, strlen(configured))) {
		return;
	}
	const struct passwd *account = getpwuid(getuid());
	if (!account) {
		return;
	}
	// the GECOS fields after the full name are office and phone numbers
	const char *gecos = account->pw_gecos ? account->pw_gecos : "";
	if (!take(name, gecos, strcspn(gecos, ","))) {
		take(name, account->pw_name, strlen(account->pw_name));
	}
}
