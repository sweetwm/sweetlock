#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "config/config.h"
#include "fs/file.h"
#include "log.h"

static bool default_path(char *path, size_t size) {
	int length = -1;
	const char *xdg = getenv("XDG_CONFIG_HOME");
	const char *home = getenv("HOME");
	// the XDG spec ignores relative values
	if (xdg && xdg[0] == '/') {
		length = snprintf(path, size, "%s/sweetlock/config.toml", xdg);
	} else if (home && home[0] == '/') {
		length = snprintf(
			path, size, "%s/.config/sweetlock/config.toml", home);
	}
	return length > 0 && (size_t)length < size;
}

static void load_file(struct config *config, const char *path, bool explicit) {
	char *buffer = NULL;
	char error[CONFIG_ERROR_SIZE] = "";
	size_t length = 0;
	enum file_result result = file_read(
		path, CONFIG_MAX_BYTES, &buffer, &length, error, sizeof error);
	bool ok = result == FILE_OK &&
		  config_parse(buffer, length, config, error, sizeof error);
	free(buffer);

	if (ok) {
		log_info("loaded config from %s", path);
	} else if (result == FILE_MISSING && !explicit) {
		log_info("no config at %s; using defaults", path);
	} else if (result == FILE_MISSING) {
		log_error("%s: no such file; using defaults", path);
	} else {
		log_error("%s: %s; using defaults", path, error);
	}
}

void config_load(struct config *config, const char *path, bool no_config) {
	config_defaults(config);
	if (no_config) {
		log_info("--no-config given; using defaults");
		return;
	}
	if (path) {
		load_file(config, path, true);
		return;
	}
	char found[PATH_MAX];
	if (!default_path(found, sizeof found)) {
		log_warn("neither XDG_CONFIG_HOME nor HOME is an absolute "
			 "path; using defaults");
		return;
	}
	load_file(config, found, false);
}

void config_override(struct config *config,
	const struct config_override *overrides, size_t count) {
	for (size_t i = 0; i < count; i++) {
		const struct config_override *o = &overrides[i];
		char error[CONFIG_ERROR_SIZE] = "";
		if (!config_set(config, o->key, o->key_length, o->value, error,
			    sizeof error)) {
			log_error("%s: %s; ignoring it", o->flag, error);
		}
	}
}
