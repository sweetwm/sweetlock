#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config/config.h"
#include "fs/file.h"
#include "fs/path.h"
#include "log.h"
#include "utf8.h"

static bool load_file(struct config *config, const char *path, bool explicit,
	char notice[static CONFIG_NOTICE_SIZE]) {
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
		return true;
	}
	if (result == FILE_MISSING && !explicit) {
		log_info("no config at %s; using defaults", path);
		return true;
	}
	if (result == FILE_MISSING) {
		snprintf(error, sizeof error, "no such file");
	}
	log_error("%s: %s; using defaults", path, error);
	// the error quotes the file, so it is cleaned like PAM's text
	char line[CONFIG_NOTICE_SIZE] = "";
	snprintf(line, sizeof line, "config ignored: %s", error);
	size_t used = utf8_line(line, strnlen(line, sizeof line), notice,
		CONFIG_NOTICE_SIZE - 1);
	notice[used] = '\0';
	return false;
}

bool config_load(struct config *config, const char *path, bool no_config,
	char notice[static CONFIG_NOTICE_SIZE]) {
	config_defaults(config);
	notice[0] = '\0';
	if (no_config) {
		log_info("--no-config given; using defaults");
		return true;
	}
	if (path) {
		return load_file(config, path, true, notice);
	}
	char base[PATH_MAX];
	char found[PATH_MAX];
	if (!path_xdg(base, "XDG_CONFIG_HOME", "/.config") ||
		!path_join(found, base, "/sweetlock/config.toml")) {
		log_warn("neither XDG_CONFIG_HOME nor HOME is an absolute "
			 "path; using defaults");
		return true;
	}
	return load_file(config, found, false, notice);
}

bool config_override(struct config *config,
	const struct config_override *overrides, size_t count) {
	bool ok = true;
	for (size_t i = 0; i < count; i++) {
		const struct config_override *o = &overrides[i];
		char error[CONFIG_ERROR_SIZE] = "";
		if (!config_set(config, o->key, o->key_length, o->value, error,
			    sizeof error)) {
			log_error("%s: %s; ignoring it", o->flag, error);
			ok = false;
		}
	}
	return ok;
}
