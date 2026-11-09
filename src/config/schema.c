#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config/config.h"
#include "config/internal.h"
#include "log.h"
#include "utf8.h"

static const struct config_blob blob_default = {
	.color = {0xff, 0xff, 0xff, 0x2e},
	.x = 50,
	.y = 50,
	.size = 36,
};

static const struct config defaults = {
	.background =
		{
			.mode = BACKGROUND_SOLID,
			.color = {0x1f, 0x1b, 0x2e, 0xff},
			.gradient =
				{
					.top_left = {0x2b, 0x23, 0x42, 0xff},
					.top_right = {0x1f, 0x1b, 0x2e, 0xff},
					.bottom_left = {0x1f, 0x1b, 0x2e, 0xff},
					.bottom_right = {0x2b, 0x23, 0x42,
						0xff},
				},
			.radial =
				{
					.center = {0x37, 0x30, 0x57, 0xff},
					.edge = {0x1f, 0x1b, 0x2e, 0xff},
					.center_x = 50,
					.center_y = 50,
					.radius = 80,
				},
			.layered =
				{
					.base = BACKGROUND_RADIAL,
					.blob_count = 2,
					.blob =
						{
							{{0x6a, 0x5a, 0x9e,
								 0x59},
								30, 35, 45},
							{{0x3e, 0x6a, 0x9a,
								 0x40},
								72, 70, 38},
						},
				},
			.image = {.path = "", .fit = FIT_FILL},
		},
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

static const char *const modes[] = {
	"solid", "gradient", "radial", "layered", "image", NULL};
// a layered background cannot be its own base
static const char *const bases[] = {"solid", "gradient", "radial", NULL};

static const char *const fits[] = {"fill", "fit", "center", "stretch", NULL};

// enum fields are written as an int
static_assert(sizeof(enum background_mode) == sizeof(int), "enum size");
static_assert(sizeof(enum background_fit) == sizeof(int), "enum size");

#define FIELD(field) offsetof(struct config, field)
#define KEY(path, type, field) {path, type, FIELD(field), 0, 0, 0, NULL}
#define INT(path, field, min, max)                                             \
	{path, CONFIG_INT, FIELD(field), 0, min, max, NULL}
#define ENUM(path, field, names)                                               \
	{path, CONFIG_ENUM, FIELD(field), 0, 0, 0, names}
#define BLOB(path, type, field, min, max)                                      \
	{path, type, FIELD(background.layered.blob[0].field),                  \
		sizeof(struct config_blob), min, max, NULL}

static const struct config_key keys[] = {
	ENUM("background.mode", background.mode, modes),
	KEY("background.color", CONFIG_COLOR, background.color),
	KEY("background.gradient.top_left", CONFIG_COLOR,
		background.gradient.top_left),
	KEY("background.gradient.top_right", CONFIG_COLOR,
		background.gradient.top_right),
	KEY("background.gradient.bottom_left", CONFIG_COLOR,
		background.gradient.bottom_left),
	KEY("background.gradient.bottom_right", CONFIG_COLOR,
		background.gradient.bottom_right),
	KEY("background.radial.center", CONFIG_COLOR, background.radial.center),
	KEY("background.radial.edge", CONFIG_COLOR, background.radial.edge),
	INT("background.radial.center_x", background.radial.center_x, 0, 100),
	INT("background.radial.center_y", background.radial.center_y, 0, 100),
	INT("background.radial.radius", background.radial.radius, 1, 200),
	ENUM("background.layered.base", background.layered.base, bases),
	BLOB("background.layered.blob.color", CONFIG_COLOR, color, 0, 0),
	BLOB("background.layered.blob.x", CONFIG_INT, x, 0, 100),
	BLOB("background.layered.blob.y", CONFIG_INT, y, 0, 100),
	BLOB("background.layered.blob.size", CONFIG_INT, size, 1, 100),
	KEY("background.image.path", CONFIG_PATH, background.image.path),
	ENUM("background.image.fit", background.image.fit, fits),
	INT("background.image.dim", background.image.dim, 0, 100),
	INT("background.image.blur", background.image.blur, 0, 100),
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

#undef BLOB
#undef ENUM
#undef INT
#undef KEY
#undef FIELD

static const char blob_array[] = "background.layered.blob";

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

bool config_key_is_array(const char *path, size_t length) {
	return length == sizeof blob_array - 1 &&
	       memcmp(path, blob_array, length) == 0;
}

void config_blobs_reset(struct config *config, int32_t count) {
	assert(count >= 0 && count <= CONFIG_BLOBS_MAX);
	config->background.layered.blob_count = count;
	// unused blobs are zero, so equal configs compare equal byte for byte
	for (int32_t i = 0; i < CONFIG_BLOBS_MAX; i++) {
		config->background.layered.blob[i] =
			i < count ? blob_default : (struct config_blob){0};
	}
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

// TOML integers arrive as text too, so both sources share this parser
static bool parse_int(const struct config_key *key, const char *text,
	size_t length, int32_t *out) {
	char digits[16];
	if (length == 0 || length >= sizeof digits ||
		(text[0] != '-' && (text[0] < '0' || text[0] > '9'))) {
		return false;
	}
	memcpy(digits, text, length);
	digits[length] = '\0';
	char *end = NULL;
	errno = 0;
	long value = strtol(digits, &end, 10);
	if (errno != 0 || end != digits + length || value < key->min ||
		value > key->max) {
		return false;
	}
	*out = (int32_t)value;
	return true;
}

static bool parse_enum(const struct config_key *key, const char *text,
	size_t length, void *out) {
	for (int i = 0; key->names[i]; i++) {
		if (equals(text, length, key->names[i])) {
			memcpy(out, &i, sizeof i);
			return true;
		}
	}
	return false;
}

// the messages spell out the limits
static_assert(CONFIG_STRING_SIZE == 128, "update the string message");
static_assert(CONFIG_PATH_SIZE == 4096, "update the path message");

static const char *expected(enum config_type type) {
	switch (type) {
	case CONFIG_BOOL:
		return "true or false";
	case CONFIG_COLOR:
		return "\"#rrggbb\" or \"#rrggbbaa\"";
	case CONFIG_STRING:
		return "a string of at most 127 bytes of UTF-8 without control "
		       "characters";
	case CONFIG_PATH:
		return "an absolute path or one starting with ~/, at most 4095 "
		       "bytes of UTF-8 without control characters";
	case CONFIG_INT:
	case CONFIG_ENUM:
		break;
	}
	return "a valid value";
}

bool config_key_fail(
	const struct config_key *key, char *error, size_t error_size) {
	if (key->type == CONFIG_INT) {
		snprintf(error, error_size,
			"%s: expected an integer from %d to %d", key->path,
			key->min, key->max);
		return false;
	}
	if (key->type != CONFIG_ENUM) {
		snprintf(error, error_size, "%s: expected %s", key->path,
			expected(key->type));
		return false;
	}
	// truncation only shortens the message
	int length =
		snprintf(error, error_size, "%s: expected one of", key->path);
	for (int i = 0;
		key->names[i] && length >= 0 && (size_t)length < error_size;
		i++) {
		length += snprintf(error + length, error_size - (size_t)length,
			"%s \"%s\"", i > 0 ? "," : "", key->names[i]);
	}
	return false;
}

static bool parse(const struct config_key *key, const char *value,
	size_t length, void *field) {
	switch (key->type) {
	case CONFIG_BOOL:
		return parse_bool(value, length, field);
	case CONFIG_INT:
		return parse_int(key, value, length, field);
	case CONFIG_ENUM:
		return parse_enum(key, value, length, field);
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
	size_t index, const char *value, size_t length, char *error,
	size_t error_size) {
	assert(index == 0 || (key->stride && index < CONFIG_BLOBS_MAX));
	char *field = (char *)config + key->offset + index * key->stride;
	return parse(key, value, length, field) ||
	       config_key_fail(key, error, error_size);
}

bool config_set(struct config *config, const char *key, size_t key_length,
	const char *value, char *error, size_t error_size) {
	const struct config_key *found = config_key_find(key, key_length);
	if (!found) {
		snprintf(error, error_size, "%.*s: unknown key",
			(int)key_length, key);
		return false;
	}
	if (found->stride) {
		snprintf(error, error_size,
			"%s: blobs can only be set in the config file",
			found->path);
		return false;
	}
	return config_key_set(
		config, found, 0, value, strlen(value), error, error_size);
}

static void log_key(const struct config_key *key, const void *field) {
	switch (key->type) {
	case CONFIG_BOOL:
		log_debug("config %s=%s", key->path,
			*(const bool *)field ? "true" : "false");
		break;
	case CONFIG_INT:
		log_debug("config %s=%d", key->path, *(const int32_t *)field);
		break;
	case CONFIG_ENUM: {
		int value = 0;
		memcpy(&value, field, sizeof value);
		log_debug("config %s=%s", key->path, key->names[value]);
		break;
	}
	case CONFIG_COLOR: {
		const struct color *c = field;
		log_debug("config %s=#%02x%02x%02x%02x", key->path, c->red,
			c->green, c->blue, c->alpha);
		break;
	}
	case CONFIG_STRING:
	case CONFIG_PATH:
		log_debug("config %s=\"%s\"", key->path, (const char *)field);
		break;
	}
}

void config_log(const struct config *config) {
	for (size_t i = 0; i < key_count; i++) {
		int32_t count = keys[i].stride
					? config->background.layered.blob_count
					: 1;
		for (int32_t j = 0; j < count; j++) {
			log_key(&keys[i], (const char *)config +
						  keys[i].offset +
						  (size_t)j * keys[i].stride);
		}
	}
}
