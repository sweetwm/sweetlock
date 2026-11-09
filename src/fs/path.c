#include "fs/path.h"

#include <stdio.h>
#include <stdlib.h>

bool path_join(char out[PATH_MAX], const char *head, const char *tail) {
	int length = snprintf(out, PATH_MAX, "%s%s", head, tail);
	if (length < 0 || length >= PATH_MAX) {
		out[0] = '\0';
		return false;
	}
	return true;
}

// relative values are ignored, as for the config path
const char *path_home(void) {
	const char *home = getenv("HOME");
	return home && home[0] == '/' ? home : NULL;
}

// the schema only lets through empty, absolute and ~/ paths
bool path_expand(char out[PATH_MAX], const char *configured, const char **why) {
	out[0] = '\0';
	*why = NULL;
	if (!configured[0]) {
		return true;
	}
	const char *head = "";
	if (configured[0] == '~') {
		head = path_home();
		if (!head) {
			*why = "~/ needs HOME to be an absolute path";
			return false;
		}
		configured++;
	}
	if (!path_join(out, head, configured)) {
		*why = "longer than PATH_MAX once ~/ is expanded";
		return false;
	}
	return true;
}
