#include "config/config.h"
#include <stdio.h>
#include <stdlib.h>

bool locky_config_user_path(char *path, size_t path_size) {
	if (path == NULL || path_size == 0) {
		return false;
	}

	const char *base = getenv("XDG_CONFIG_HOME");
	if (base != NULL && base[0] != '\0') {
		int len =
			snprintf(path, path_size, "%s/locky/config.toml", base);
		return len > 0 && (size_t)len < path_size;
	}

	const char *home = getenv("HOME");
	if (home == NULL || home[0] == '\0') {
		return false;
	}
	int len =
		snprintf(path, path_size, "%s/.config/locky/config.toml", home);
	return len > 0 && (size_t)len < path_size;
}
