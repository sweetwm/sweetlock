#include "user/user.h"
#include "app/log.h"
#include "render/image.h"
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define AVATAR_LOAD_SIZE 256
#define FACE_PATH_MAX 4096

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

static void load_face_avatar(struct locky_state *state, const char *home) {
	if (home == NULL || home[0] == '\0') {
		return;
	}

	char path[FACE_PATH_MAX] = {0};
	int len = snprintf(path, sizeof(path), "%s/.face", home);
	if (len <= 0 || (size_t)len >= sizeof(path)) {
		return;
	}
	if (access(path, R_OK) != 0) {
		return;
	}

	state->avatar_surface = locky_image_load(
		path, AVATAR_LOAD_SIZE, AVATAR_LOAD_SIZE, true);
}

static void load_config_avatar(struct locky_state *state) {
	if (!state->config.has_avatar_image) {
		return;
	}
	if (access(state->config.avatar_image, R_OK) != 0) {
		locky_log(LOCKY_ERROR, "avatar image is not readable");
		return;
	}
	state->avatar_surface = locky_image_load(state->config.avatar_image,
		AVATAR_LOAD_SIZE, AVATAR_LOAD_SIZE, true);
	if (state->avatar_surface == NULL) {
		locky_log(LOCKY_ERROR, "failed to load avatar image");
	}
}

void locky_user_init(struct locky_state *state) {
	copy_name(state->username, "User");
	const char *home = getenv("HOME");

	struct passwd pwd = {0};
	struct passwd *result = NULL;
	long buf_size = sysconf(_SC_GETPW_R_SIZE_MAX);
	if (buf_size < 1024) {
		buf_size = 1024;
	}

	char *buf = malloc((size_t)buf_size);
	if (buf == NULL) {
		copy_name(state->username, getenv("USER"));
		load_config_avatar(state);
		if (state->avatar_surface == NULL) {
			load_face_avatar(state, home);
		}
		return;
	}

	if (getpwuid_r(getuid(), &pwd, buf, (size_t)buf_size, &result) == 0 &&
		result != NULL) {
		copy_name(state->username, pwd.pw_name);
		copy_gecos_name(state->username, pwd.pw_gecos);
		if (home == NULL || home[0] == '\0') {
			home = pwd.pw_dir;
		}
	} else {
		copy_name(state->username, getenv("USER"));
	}

	load_config_avatar(state);
	if (state->avatar_surface == NULL) {
		load_face_avatar(state, home);
	}
	free(buf);
}

void locky_user_finish(struct locky_state *state) {
	if (state->avatar_surface != NULL) {
		cairo_surface_destroy(state->avatar_surface);
		state->avatar_surface = NULL;
	}
}
