#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config/config.h"
#include "config/internal.h"
#include "tomlc17.h"

enum {
	// longer than any key path in the schema
	PATH_SIZE = 128,
};

struct walk {
	struct config *config;
	char path[PATH_SIZE];
	size_t index;
	char *error;
	size_t error_size;
};

__attribute__((format(printf, 2, 3))) static bool fail(
	struct walk *w, const char *format, ...) {
	va_list args;
	va_start(args, format);
	vsnprintf(w->error, w->error_size, format, args);
	va_end(args);
	return false;
}

// a key with control bytes is shown with them replaced, so the log stays one
// readable line
static bool unknown(struct walk *w, size_t length) {
	for (size_t i = 0; i < length; i++) {
		if ((unsigned char)w->path[i] < 0x20) {
			w->path[i] = '?';
		}
	}
	return fail(w, "%.*s: unknown key", (int)length, w->path);
}

// TOML booleans and integers go through the setter as text, the same path
// as a flag
static bool value(
	struct walk *w, const struct config_key *key, toml_datum_t datum) {
	char text[32] = "";
	const char *source = text;
	size_t length = 0;
	if (key->type == CONFIG_BOOL && datum.type == TOML_BOOLEAN) {
		length = (size_t)snprintf(text, sizeof text, "%s",
			datum.u.boolean ? "true" : "false");
	} else if (key->type == CONFIG_INT && datum.type == TOML_INT64) {
		length = (size_t)snprintf(
			text, sizeof text, "%" PRId64, datum.u.int64);
	} else if (key->type != CONFIG_BOOL && key->type != CONFIG_INT &&
		   datum.type == TOML_STRING) {
		source = datum.u.str.ptr;
		length = (size_t)datum.u.str.len;
	} else {
		return config_key_fail(key, w->error, w->error_size);
	}
	return config_key_set(w->config, key, w->index, source, length,
		w->error, w->error_size);
}

static bool table(struct walk *w, size_t prefix, toml_datum_t datum);

// the array replaces the default blobs; a key it leaves out keeps the blob
// default
static bool array(struct walk *w, size_t end, toml_datum_t datum) {
	if (datum.type != TOML_ARRAY) {
		return fail(w, "%s: expected an array of tables", w->path);
	}
	if (datum.u.arr.size > CONFIG_BLOBS_MAX) {
		return fail(w, "%s: expected at most %d tables", w->path,
			CONFIG_BLOBS_MAX);
	}
	config_blobs_reset(w->config, datum.u.arr.size);
	for (int32_t i = 0; i < datum.u.arr.size; i++) {
		// the last blob's walk left its key on the path
		w->path[end] = '\0';
		if (datum.u.arr.elem[i].type != TOML_TABLE) {
			return fail(
				w, "%s: expected an array of tables", w->path);
		}
		w->index = (size_t)i;
		bool ok = table(w, end, datum.u.arr.elem[i]);
		w->index = 0;
		if (!ok) {
			return false;
		}
	}
	return true;
}

// recursion only enters tables that hold schema keys, so the schema bounds
// its depth
static bool table(struct walk *w, size_t prefix, toml_datum_t datum) {
	for (int32_t i = 0; i < datum.u.tab.size; i++) {
		const char *key = datum.u.tab.key[i];
		size_t length = (size_t)datum.u.tab.len[i];
		size_t dot = prefix > 0;
		size_t room = PATH_SIZE - 1 - prefix - dot;
		size_t copied = length < room ? length : room;
		if (dot) {
			w->path[prefix] = '.';
		}
		memcpy(w->path + prefix + dot, key, copied);
		size_t end = prefix + dot + copied;
		w->path[end] = '\0';
		// a quoted key with a dot must not reach a nested key
		if (copied < length || length == 0 ||
			memchr(key, '.', length)) {
			return unknown(w, end);
		}

		toml_datum_t child = datum.u.tab.value[i];
		const struct config_key *found = config_key_find(w->path, end);
		if (config_key_is_array(w->path, end)) {
			if (!array(w, end, child)) {
				return false;
			}
		} else if (found) {
			if (!value(w, found, child)) {
				return false;
			}
		} else if (config_key_in_table(w->path, end)) {
			if (child.type != TOML_TABLE) {
				return fail(w, "%s: expected a table", w->path);
			}
			if (!table(w, end, child)) {
				return false;
			}
		} else {
			return unknown(w, end);
		}
	}
	return true;
}

bool config_parse(const char *text, size_t length, struct config *config,
	char *error, size_t error_size) {
	struct walk w = {
		.error = error,
		.error_size = error_size,
	};
	if (length > CONFIG_MAX_BYTES) {
		return fail(&w, "expected at most %d bytes", CONFIG_MAX_BYTES);
	}
	if (length > 0 && memchr(text, '\0', length)) {
		return fail(&w, "contains a NUL byte");
	}
	// tomlc17 wants a NUL after the text
	char *copy = malloc(length + 1);
	if (!copy) {
		return fail(&w, "out of memory");
	}
	if (length > 0) {
		memcpy(copy, text, length);
	}
	copy[length] = '\0';

	struct config candidate = *config;
	w.config = &candidate;
	toml_option_t option = toml_default_option();
	option.check_utf8 = true;
	toml_set_option(option);
	toml_result_t result = toml_parse(copy, (int)length);
	bool ok = result.ok ? table(&w, 0, result.toptab)
			    : fail(&w, "%s", result.errmsg);
	toml_free(result);
	free(copy);
	if (ok) {
		*config = candidate;
	}
	return ok;
}
