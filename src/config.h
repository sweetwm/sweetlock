#ifndef SWEETLOCK_CONFIG_H
#define SWEETLOCK_CONFIG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
	CONFIG_MAX_BYTES = 64 * 1024,
	CONFIG_ERROR_SIZE = 256,
};

struct color {
	uint8_t red;
	uint8_t green;
	uint8_t blue;
	uint8_t alpha;
};

struct config {
	struct {
		struct color color;
	} background;
};

// one --set or alias, with key and value pointing into argv
struct config_override {
	const char *flag;
	const char *key;
	size_t key_length;
	const char *value;
};

void config_defaults(struct config *config);

bool config_parse(const char *text, size_t length, struct config *config,
	char *error, size_t error_size);

// applies one scalar key given as flag text, through the same schema
bool config_set(struct config *config, const char *key, size_t key_length,
	const char *value, char *error, size_t error_size);

// defaults, then the file unless no_config; never fails, errors are logged
void config_load(struct config *config, const char *path, bool no_config);

// applies each valid override and logs the rest
void config_override(struct config *config,
	const struct config_override *overrides, size_t count);

void config_log(const struct config *config);

#endif
