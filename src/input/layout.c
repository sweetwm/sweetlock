#include "input/layout.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <xkbcommon/xkbcommon.h>

#include "fs/path.h"

// the keyboard's context has no include paths, since keymaps arrive whole
enum file_result layout_list_load(char path[PATH_MAX], char **data,
	size_t *length, char *error, size_t error_size) {
	enum file_result result = FILE_MISSING;
	char tried[PATH_MAX] = "";
	struct xkb_context *xkb =
		xkb_context_new(XKB_CONTEXT_NO_DEFAULT_INCLUDES);
	if (!xkb) {
		snprintf(error, error_size, "failed to create an xkb context");
		path[0] = '\0';
		return FILE_FAILED;
	}
	// its own logger writes to stderr, and a missing path is no news
	xkb_context_set_log_level(xkb, XKB_LOG_LEVEL_CRITICAL);
	xkb_context_include_path_append_default(xkb);
	unsigned int count = xkb_context_num_include_paths(xkb);
	for (unsigned int i = 0; i < count; i++) {
		const char *root = xkb_context_include_path_get(xkb, i);
		if (!root || !path_join(path, root, "/rules/evdev.lst")) {
			continue;
		}
		enum file_result found = file_read(
			path, LAYOUT_LIST_MAX, data, length, error, error_size);
		if (found == FILE_OK) {
			xkb_context_unref(xkb);
			return FILE_OK;
		}
		if (found == FILE_FAILED) {
			result = FILE_FAILED;
			memcpy(tried, path, sizeof tried);
		}
	}
	xkb_context_unref(xkb);
	memcpy(path, tried, PATH_MAX);
	return result;
}

static bool lower(char c) {
	return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
}

static bool letter(char c) {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static void upper(const char *code, size_t length, char *label, size_t size) {
	size_t n = 0;
	for (size_t i = 0; i < length && n + 1 < size; i++) {
		char c = code[i];
		label[n++] = c >= 'a' && c <= 'z' ? (char)(c - 'a' + 'A') : c;
	}
	label[n] = '\0';
}

static size_t span(const char *text, size_t length, bool (*accept)(char)) {
	size_t n = 0;
	while (n < length && accept(text[n])) {
		n++;
	}
	return n;
}

static bool space(char c) {
	return c == ' ' || c == '\t';
}

static bool word(char c) {
	return c != '\0' && !space(c);
}

enum section { SECTION_OTHER, SECTION_LAYOUT, SECTION_VARIANT };

static enum section section_of(const char *line, size_t length) {
	if (length == 8 && memcmp(line, "! layout", 8) == 0) {
		return SECTION_LAYOUT;
	}
	if (length == 9 && memcmp(line, "! variant", 9) == 0) {
		return SECTION_VARIANT;
	}
	return SECTION_OTHER;
}

// "  lv              Latvian" or, for a variant,
// "  apostrophe      lv: Latvian (apostrophe)"
static bool matches(const char *line, size_t length, enum section section,
	const char *name, size_t name_length, const char **code,
	size_t *code_length) {
	size_t at = span(line, length, space);
	size_t first = span(line + at, length - at,
		section == SECTION_VARIANT ? word : lower);
	if (first == 0) {
		return false;
	}
	*code = line + at;
	*code_length = first;
	at += first;
	size_t gap = span(line + at, length - at, space);
	if (gap == 0) {
		return false;
	}
	at += gap;
	if (section == SECTION_VARIANT) {
		*code = line + at;
		*code_length = span(line + at, length - at, lower);
		at += *code_length;
		if (*code_length == 0 || length - at < 2 ||
			memcmp(line + at, ": ", 2) != 0) {
			return false;
		}
		at += 2;
	}
	return length - at == name_length &&
	       memcmp(line + at, name, name_length) == 0;
}

void layout_label(const char *list, size_t length, const char *name,
	char *label, size_t size) {
	if (size == 0) {
		return;
	}
	size_t name_length = strlen(name);
	enum section section = SECTION_OTHER;
	size_t start = 0;
	while (name_length > 0 && start < length) {
		const char *line = list + start;
		const char *end = memchr(line, '\n', length - start);
		size_t line_length =
			end ? (size_t)(end - line) : length - start;
		start += line_length + 1;
		const char *code = NULL;
		size_t code_length = 0;
		if (line_length > 0 && line[0] == '!') {
			section = section_of(line, line_length);
		} else if (section != SECTION_OTHER &&
			   matches(line, line_length, section, name,
				   name_length, &code, &code_length)) {
			upper(code, code_length, label, size);
			return;
		}
	}
	// a custom keymap's layout is in no list
	size_t n = 0;
	char guess[2];
	for (size_t i = 0; i < name_length && n < sizeof guess; i++) {
		if (letter(name[i])) {
			guess[n++] = name[i];
		}
	}
	upper(guess, n, label, size);
}
