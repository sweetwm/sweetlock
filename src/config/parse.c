#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "config/internal.h"
#include "tomlc17.h"

enum {
	// longer than any key path in the schema
	PATH_SIZE = 128,
};

struct walk {
	struct config *config;
	char path[PATH_SIZE];
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

// a TOML boolean goes through the setter as text, the same path as a flag
static bool value(
	struct walk *w, const struct config_key *key, toml_datum_t datum) {
	if (key->type == CONFIG_BOOL && datum.type == TOML_BOOLEAN) {
		const char *text = datum.u.boolean ? "true" : "false";
		return config_key_set(w->config, key, text, strlen(text),
			w->error, w->error_size);
	}
	if (key->type == CONFIG_BOOL || datum.type != TOML_STRING) {
		return fail(w, "%s: %s", key->path, config_key_expected(key));
	}
	return config_key_set(w->config, key, datum.u.str.ptr,
		(size_t)datum.u.str.len, w->error, w->error_size);
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
		if (found) {
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
