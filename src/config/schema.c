#include <stdio.h>
#include <string.h>

#include "config.h"
#include "config/internal.h"
#include "log.h"

static const struct config defaults = {
	.background = {.color = {0x1f, 0x1b, 0x2e, 0xff}},
};

static const struct config_key keys[] = {
	{"background.color", CONFIG_COLOR,
		offsetof(struct config, background.color)},
};

static const size_t key_count = sizeof keys / sizeof keys[0];

void config_defaults(struct config *config) {
	*config = defaults;
}

const struct config_key *config_key_find(const char *path, size_t length) {
	for (size_t i = 0; i < key_count; i++) {
		if (strlen(keys[i].path) == length &&
			memcmp(keys[i].path, path, length) == 0) {
			return &keys[i];
		}
	}
	return NULL;
}

bool config_key_in_table(const char *path, size_t length) {
	for (size_t i = 0; i < key_count; i++) {
		if (strlen(keys[i].path) > length &&
			memcmp(keys[i].path, path, length) == 0 &&
			keys[i].path[length] == '.') {
			return true;
		}
	}
	return false;
}

static int hex(char c) {
	if (c >= '0' && c <= '9') {
		return c - '0';
	}
	if (c >= 'a' && c <= 'f') {
		return c - 'a' + 10;
	}
	if (c >= 'A' && c <= 'F') {
		return c - 'A' + 10;
	}
	return -1;
}

static bool parse_color(const char *text, size_t length, struct color *out) {
	if ((length != 7 && length != 9) || text[0] != '#') {
		return false;
	}
	uint8_t channels[4] = {0, 0, 0, 0xff};
	for (size_t i = 1; i < length; i += 2) {
		int high = hex(text[i]);
		int low = hex(text[i + 1]);
		if (high < 0 || low < 0) {
			return false;
		}
		channels[i / 2] = (uint8_t)(high * 16 + low);
	}
	*out = (struct color){
		channels[0], channels[1], channels[2], channels[3]};
	return true;
}

bool config_key_set(struct config *config, const struct config_key *key,
	const char *value, size_t length, char *error, size_t error_size) {
	void *field = (char *)config + key->offset;
	switch (key->type) {
	case CONFIG_COLOR:
		if (!parse_color(value, length, field)) {
			snprintf(error, error_size,
				"%s: expected \"#rrggbb\" or \"#rrggbbaa\"",
				key->path);
			return false;
		}
		return true;
	}
	return false;
}

bool config_set(struct config *config, const char *key, size_t key_length,
	const char *value, char *error, size_t error_size) {
	const struct config_key *found = config_key_find(key, key_length);
	if (!found) {
		snprintf(error, error_size, "%.*s: unknown key",
			(int)key_length, key);
		return false;
	}
	return config_key_set(
		config, found, value, strlen(value), error, error_size);
}

void config_log(const struct config *config) {
	for (size_t i = 0; i < key_count; i++) {
		const void *field = (const char *)config + keys[i].offset;
		switch (keys[i].type) {
		case CONFIG_COLOR: {
			const struct color *c = field;
			log_debug("config %s=#%02x%02x%02x%02x", keys[i].path,
				c->red, c->green, c->blue, c->alpha);
			break;
		}
		}
	}
}
