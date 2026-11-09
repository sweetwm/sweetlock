#ifndef SWEETLOCK_CONFIG_ARGS_H
#define SWEETLOCK_CONFIG_ARGS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "config/config.h"

enum {
	ARGS_OVERRIDE_MAX = 32,
	// per side, in pixels
	ARGS_PREVIEW_SIZE_MAX = 8192,
};

enum args_action {
	ARGS_RUN,
	ARGS_PREVIEW,
	ARGS_HELP,
	ARGS_VERSION,
};

// what the password field shows in a preview
enum preview_field {
	PREVIEW_IDLE,
	PREVIEW_TYPING,
	PREVIEW_REVEALED,
	PREVIEW_CHECKING,
	PREVIEW_FAILED,
};

struct preview {
	const char *path;
	enum preview_field field;
	// the output's size in pixels, and its scale in 120ths
	int32_t width;
	int32_t height;
	uint32_t scale;
};

struct args {
	enum args_action action;
	const char *config_path;
	bool no_config;
	bool daemonize;
	struct preview preview;
	// in command-line order, so a later flag wins
	struct config_override overrides[ARGS_OVERRIDE_MAX];
	size_t override_count;
};

extern const char args_usage[];

// a bad argument is logged and skipped, never fatal
void args_parse(struct args *args, int argc, char **argv);

#endif
