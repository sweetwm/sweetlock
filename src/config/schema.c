#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "config/internal.h"
#include "log.h"
#include "utf8.h"

static const struct config defaults = {
	.background = {.color = {0x1f, 0x1b, 0x2e, 0xff}},
	.clock = {.enabled = true, .format = "%H:%M"},
	.date = {.enabled = true, .format = "%A, %B %-d"},
	.avatar = {.enabled = true, .path = ""},
	.username = {.enabled = true, .text = ""},
	.input = {.placeholder = "Password", .reveal_toggle = true},
	.theme =
		{
			.font = "sans-serif",
			.foreground = {0xe8, 0xe6, 0xf0, 0xff},
			.muted = {0xa9, 0xa4, 0xbd, 0xff},
			.field = {0xff, 0xff, 0xff, 0x1a},
			.error = {0xe0, 0x6c, 0x75, 0xff},
		},
};

#define KEY(path, type, field) {path, type, offsetof(struct config, field)}

static const struct config_key keys[] = {
	KEY("background.color", CONFIG_COLOR, background.color),
	KEY("clock.enabled", CONFIG_BOOL, clock.enabled),
	KEY("clock.format", CONFIG_STRING, clock.format),
	KEY("date.enabled", CONFIG_BOOL, date.enabled),
	KEY("date.format", CONFIG_STRING, date.format),
	KEY("avatar.enabled", CONFIG_BOOL, avatar.enabled),
	KEY("avatar.path", CONFIG_PATH, avatar.path),
	KEY("username.enabled", CONFIG_BOOL, username.enabled),
	KEY("username.text", CONFIG_STRING, username.text),
	KEY("input.placeholder", CONFIG_STRING, input.placeholder),
	KEY("input.reveal_toggle", CONFIG_BOOL, input.reveal_toggle),
	KEY("theme.font", CONFIG_STRING, theme.font),
	KEY("theme.foreground", CONFIG_COLOR, theme.foreground),
	KEY("theme.muted", CONFIG_COLOR, theme.muted),
	KEY("theme.field", CONFIG_COLOR, theme.field),
	KEY("theme.error", CONFIG_COLOR, theme.error),
};

#undef KEY

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

static bool equals(const char *text, size_t length, const char *word) {
	return strlen(word) == length && memcmp(text, word, length) == 0;
}

static bool parse_bool(const char *text, size_t length, bool *out) {
	if (equals(text, length, "true") || equals(text, length, "false")) {
		*out = length == 4;
		return true;
	}
	return false;
}

// the text is shown, logged or handed to strftime and Pango, so no controls
static bool parse_text(
	const char *text, size_t length, char *out, size_t size) {
	if (length >= size || !utf8_is_plain(text, length)) {
		return false;
	}
	memcpy(out, text, length);
	// zeroed past the end, so two equal configs compare equal byte for byte
	memset(out + length, 0, size - length);
	return true;
}

static bool parse_path(const char *text, size_t length, char *out) {
	bool anchored = length == 0 || text[0] == '/' ||
			(length >= 2 && text[0] == '~' && text[1] == '/');
	return anchored && parse_text(text, length, out, CONFIG_PATH_SIZE);
}

// the messages spell out the limits
static_assert(CONFIG_STRING_SIZE == 128, "update the string message");
static_assert(CONFIG_PATH_SIZE == 4096, "update the path message");

const char *config_key_expected(const struct config_key *key) {
	switch (key->type) {
	case CONFIG_BOOL:
		return "expected true or false";
	case CONFIG_COLOR:
		return "expected \"#rrggbb\" or \"#rrggbbaa\"";
	case CONFIG_STRING:
		return "expected a string of at most 127 bytes of UTF-8 "
		       "without control characters";
	case CONFIG_PATH:
		return "expected an absolute path or one starting with ~/, at "
		       "most 4095 bytes of UTF-8 without control characters";
	}
	return "unexpected value";
}

static bool parse(const struct config_key *key, const char *value,
	size_t length, void *field) {
	switch (key->type) {
	case CONFIG_BOOL:
		return parse_bool(value, length, field);
	case CONFIG_COLOR:
		return parse_color(value, length, field);
	case CONFIG_STRING:
		return parse_text(value, length, field, CONFIG_STRING_SIZE);
	case CONFIG_PATH:
		return parse_path(value, length, field);
	}
	return false;
}

bool config_key_set(struct config *config, const struct config_key *key,
	const char *value, size_t length, char *error, size_t error_size) {
	if (!parse(key, value, length, (char *)config + key->offset)) {
		snprintf(error, error_size, "%s: %s", key->path,
			config_key_expected(key));
		return false;
	}
	return true;
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
		case CONFIG_BOOL:
			log_debug("config %s=%s", keys[i].path,
				*(const bool *)field ? "true" : "false");
			break;
		case CONFIG_COLOR: {
			const struct color *c = field;
			log_debug("config %s=#%02x%02x%02x%02x", keys[i].path,
				c->red, c->green, c->blue, c->alpha);
			break;
		}
		case CONFIG_STRING:
		case CONFIG_PATH:
			log_debug("config %s=\"%s\"", keys[i].path,
				(const char *)field);
			break;
		}
	}
}
