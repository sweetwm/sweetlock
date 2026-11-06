#ifndef SWEETLOCK_ARGS_H
#define SWEETLOCK_ARGS_H

#include <stdbool.h>
#include <stddef.h>

#include "config.h"

enum {
	ARGS_OVERRIDE_MAX = 32,
};

enum args_action {
	ARGS_RUN,
	ARGS_HELP,
	ARGS_VERSION,
};

struct args {
	enum args_action action;
	const char *config_path;
	bool no_config;
	// in command-line order, so a later flag wins
	struct config_override overrides[ARGS_OVERRIDE_MAX];
	size_t override_count;
};

extern const char args_usage[];

// a bad argument is logged and skipped, never fatal
void args_parse(struct args *args, int argc, char **argv);

#endif
