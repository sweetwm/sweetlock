#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "config.h"
#include "log.h"

enum read_result {
	READ_OK,
	READ_MISSING,
	READ_FAILED,
};

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

static enum read_result fail_read(char *error, size_t size, const char *what) {
	snprintf(error, size, "%s", what);
	return READ_FAILED;
}

// O_NONBLOCK so a FIFO at the path cannot hang the lock; it is then refused
static enum read_result read_file(const char *path, char *buffer,
	size_t *length, char *error, size_t size) {
	int fd = open(path, O_RDONLY | O_CLOEXEC | O_NONBLOCK | O_NOCTTY);
	if (fd < 0) {
		return errno == ENOENT
			       ? READ_MISSING
			       : fail_read(error, size, strerror(errno));
	}
	struct stat info;
	if (fstat(fd, &info) != 0 || !S_ISREG(info.st_mode)) {
		close(fd);
		return fail_read(error, size, "not a regular file");
	}
	size_t total = 0;
	for (;;) {
		ssize_t n =
			read(fd, buffer + total, CONFIG_MAX_BYTES + 1 - total);
		if (n < 0 && errno == EINTR) {
			continue;
		}
		if (n < 0) {
			int saved = errno;
			close(fd);
			return fail_read(error, size, strerror(saved));
		}
		total += (size_t)n;
		if (n == 0 || total > CONFIG_MAX_BYTES) {
			break;
		}
	}
	close(fd);
	if (total > CONFIG_MAX_BYTES) {
		snprintf(error, size, "larger than %d bytes", CONFIG_MAX_BYTES);
		return READ_FAILED;
	}
	*length = total;
	return READ_OK;
}

static void load_file(struct config *config, const char *path, bool explicit) {
	char *buffer = malloc(CONFIG_MAX_BYTES + 1);
	if (!buffer) {
		log_error("out of memory reading the config; using defaults");
		return;
	}
	char error[CONFIG_ERROR_SIZE] = "";
	size_t length = 0;
	enum read_result result =
		read_file(path, buffer, &length, error, sizeof error);
	bool ok = result == READ_OK &&
		  config_parse(buffer, length, config, error, sizeof error);
	free(buffer);

	if (ok) {
		log_info("loaded config from %s", path);
	} else if (result == READ_MISSING && !explicit) {
		log_info("no config at %s; using defaults", path);
	} else if (result == READ_MISSING) {
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
