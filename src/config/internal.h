#ifndef SWEETLOCK_CONFIG_INTERNAL_H
#define SWEETLOCK_CONFIG_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "config.h"

enum config_type {
	CONFIG_BOOL,
	CONFIG_COLOR,
	CONFIG_STRING,
	// empty, absolute, or under ~/
	CONFIG_PATH,
};

struct config_key {
	const char *path;
	enum config_type type;
	size_t offset;
};

// lengths, not strcmp, so a key with an escaped NUL never matches a name
const struct config_key *config_key_find(const char *path, size_t length);

// whether some key lives under the table at path
bool config_key_in_table(const char *path, size_t length);

// what a value of the key's type looks like, for error messages
const char *config_key_expected(const struct config_key *key);

// value is text of the given length, from TOML or from a flag
bool config_key_set(struct config *config, const struct config_key *key,
	const char *value, size_t length, char *error, size_t error_size);

#endif
