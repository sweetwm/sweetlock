#ifndef SWEETLOCK_CONFIG_INTERNAL_H
#define SWEETLOCK_CONFIG_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "config/config.h"

enum config_type {
	CONFIG_BOOL,
	CONFIG_INT,
	CONFIG_ENUM,
	CONFIG_COLOR,
	CONFIG_STRING,
	// empty, absolute, or under ~/
	CONFIG_PATH,
};

struct config_key {
	const char *path;
	enum config_type type;
	size_t offset;
	size_t stride;
	int32_t min;
	int32_t max;
	const char *const *names;
};

// lengths, not strcmp, so a key with an escaped NUL never matches a name
const struct config_key *config_key_find(const char *path, size_t length);

// whether some key lives under the table at path
bool config_key_in_table(const char *path, size_t length);

// whether path is background.layered.blob, the one array of tables
bool config_key_is_array(const char *path, size_t length);

void config_blobs_reset(struct config *config, int32_t count);

bool config_key_fail(
	const struct config_key *key, char *error, size_t error_size);

bool config_key_set(struct config *config, const struct config_key *key,
	size_t index, const char *value, size_t length, char *error,
	size_t error_size);

#endif
