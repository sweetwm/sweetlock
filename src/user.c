#include "locky/user.h"
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void copy_name(char dst[static LOCKY_USERNAME_MAX], const char *src) {
	if (src == NULL || src[0] == '\0') {
		return;
	}
	snprintf(dst, LOCKY_USERNAME_MAX, "%s", src);
}

static void copy_gecos_name(
	char dst[static LOCKY_USERNAME_MAX], const char *gecos) {
	if (gecos == NULL || gecos[0] == '\0') {
		return;
	}

	size_t len = strcspn(gecos, ",");
	if (len == 0) {
		return;
	}
	if (len >= LOCKY_USERNAME_MAX) {
		len = LOCKY_USERNAME_MAX - 1;
	}

	memcpy(dst, gecos, len);
	dst[len] = '\0';
}

void locky_user_init(struct locky_state *state) {
	copy_name(state->username, "User");

	struct passwd pwd = {0};
	struct passwd *result = NULL;
	long buf_size = sysconf(_SC_GETPW_R_SIZE_MAX);
	if (buf_size < 1024) {
		buf_size = 1024;
	}

	char *buf = malloc((size_t)buf_size);
	if (buf == NULL) {
		copy_name(state->username, getenv("USER"));
		return;
	}

	if (getpwuid_r(getuid(), &pwd, buf, (size_t)buf_size, &result) == 0 &&
		result != NULL) {
		copy_name(state->username, pwd.pw_name);
		copy_gecos_name(state->username, pwd.pw_gecos);
	} else {
		copy_name(state->username, getenv("USER"));
	}

	free(buf);
}
