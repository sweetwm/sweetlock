#include "config/config.h"
#include "app/log.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_BACKGROUND 0x1E1E2E
#define CONFIG_LINE_MAX 4096

enum config_section {
	SECTION_ROOT,
	SECTION_BACKGROUND,
	SECTION_USER,
	SECTION_UNKNOWN,
};

static bool ascii_space(char ch) {
	return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' ||
	       ch == '\v' || ch == '\f';
}

static bool ascii_xdigit(char ch) {
	return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') ||
	       (ch >= 'A' && ch <= 'F');
}

static char *trim(char *str) {
	while (ascii_space(*str)) {
		str++;
	}
	if (*str == '\0') {
		return str;
	}
	char *end = str + strlen(str) - 1;
	while (end > str && ascii_space(*end)) {
		*end = '\0';
		end--;
	}
	return str;
}

static void strip_comment(char *line) {
	char quote = '\0';
	bool escape = false;
	for (char *p = line; *p != '\0'; p++) {
		if (quote != '\0') {
			if (escape) {
				escape = false;
			} else if (quote == '"' && *p == '\\') {
				escape = true;
			} else if (*p == quote) {
				quote = '\0';
			}
		} else if (*p == '"' || *p == '\'') {
			quote = *p;
		} else if (*p == '#') {
			*p = '\0';
			return;
		}
	}
}

static bool copy_path_value(char *dst, size_t dst_size, const char *src) {
	if (src[0] == '~' && src[1] == '/') {
		const char *home = getenv("HOME");
		if (home == NULL || home[0] == '\0') {
			return false;
		}
		int len = snprintf(dst, dst_size, "%s/%s", home, src + 2);
		return len > 0 && (size_t)len < dst_size;
	}
	int len = snprintf(dst, dst_size, "%s", src);
	return len >= 0 && (size_t)len < dst_size;
}

static bool parse_string(char *value, char *dst, size_t dst_size) {
	value = trim(value);
	char quote = value[0];
	if (quote != '"' && quote != '\'') {
		return false;
	}
	size_t len = 0;
	for (char *p = value + 1; *p != '\0'; p++) {
		if (*p == quote) {
			char *rest = trim(p + 1);
			if (*rest != '\0' || len >= dst_size) {
				return false;
			}
			dst[len] = '\0';
			return true;
		}
		char out = *p;
		if (quote == '"' && *p == '\\') {
			p++;
			if (*p == '"' || *p == '\\') {
				out = *p;
			} else if (*p == 'n') {
				out = '\n';
			} else if (*p == 't') {
				out = '\t';
			} else if (*p == 'r') {
				out = '\r';
			} else {
				return false;
			}
		}
		if (len + 1 >= dst_size) {
			return false;
		}
		dst[len++] = out;
	}
	return false;
}

static bool parse_color(char *value, uint32_t *color) {
	char buf[16] = {0};
	if (!parse_string(value, buf, sizeof(buf))) {
		return false;
	}
	const char *hex = buf;
	if (hex[0] == '#') {
		hex++;
	}
	if (strlen(hex) != 6) {
		return false;
	}
	for (size_t i = 0; i < 6; i++) {
		if (!ascii_xdigit(hex[i])) {
			return false;
		}
	}

	char *end = NULL;
	unsigned long parsed = strtoul(hex, &end, 16);
	if (end == NULL || *end != '\0' || parsed > 0xFFFFFFUL) {
		return false;
	}
	*color = (uint32_t)parsed;
	return true;
}

static bool parse_path(char *value, char *dst, size_t dst_size) {
	char path[LOCKY_CONFIG_PATH_MAX] = {0};
	return parse_string(value, path, sizeof(path)) &&
	       copy_path_value(dst, dst_size, path);
}

static enum config_section parse_section(char *line) {
	size_t len = strlen(line);
	if (len < 3 || line[0] != '[' || line[len - 1] != ']') {
		return SECTION_UNKNOWN;
	}
	line[len - 1] = '\0';
	char *name = trim(line + 1);
	if (strcmp(name, "background") == 0) {
		return SECTION_BACKGROUND;
	}
	if (strcmp(name, "user") == 0) {
		return SECTION_USER;
	}
	return SECTION_UNKNOWN;
}

static void parse_background(struct locky_config *config, char *key,
	char *value, unsigned int line) {
	if (strcmp(key, "color") == 0) {
		uint32_t color = 0;
		if (parse_color(value, &color)) {
			config->background_color = color;
		} else {
			locky_log(LOCKY_ERROR,
				"invalid background.color on config line %u",
				line);
		}
	} else if (strcmp(key, "image") == 0) {
		if (parse_path(value, config->background_image,
			    sizeof(config->background_image))) {
			config->has_background_image = true;
		} else {
			locky_log(LOCKY_ERROR,
				"invalid background.image on config line %u",
				line);
		}
	} else {
		locky_log(LOCKY_INFO,
			"unknown background key on config line %u", line);
	}
}

static void parse_user(struct locky_config *config, char *key, char *value,
	unsigned int line) {
	if (strcmp(key, "avatar") == 0) {
		if (parse_path(value, config->avatar_image,
			    sizeof(config->avatar_image))) {
			config->has_avatar_image = true;
		} else {
			locky_log(LOCKY_ERROR,
				"invalid user.avatar on config line %u", line);
		}
	} else {
		locky_log(
			LOCKY_INFO, "unknown user key on config line %u", line);
	}
}

static void parse_entry(struct locky_config *config,
	enum config_section section, char *line, unsigned int line_number) {
	char *eq = strchr(line, '=');
	if (eq == NULL) {
		locky_log(LOCKY_ERROR, "invalid config entry on line %u",
			line_number);
		return;
	}
	*eq = '\0';
	char *key = trim(line);
	char *value = trim(eq + 1);
	if (*key == '\0' || *value == '\0') {
		locky_log(LOCKY_ERROR, "invalid config entry on line %u",
			line_number);
		return;
	}

	if (section == SECTION_BACKGROUND) {
		parse_background(config, key, value, line_number);
	} else if (section == SECTION_USER) {
		parse_user(config, key, value, line_number);
	} else {
		locky_log(LOCKY_INFO, "unknown config key on line %u",
			line_number);
	}
}

void locky_config_defaults(struct locky_config *config) {
	*config = (struct locky_config){
		.background_color = DEFAULT_BACKGROUND,
	};
}

bool locky_config_load_path(struct locky_config *config, const char *path) {
	if (config == NULL || path == NULL || path[0] == '\0') {
		return false;
	}

	FILE *file = fopen(path, "r");
	if (file == NULL) {
		if (errno != ENOENT) {
			locky_log(LOCKY_ERROR, "failed to read config file");
		}
		return true;
	}

	enum config_section section = SECTION_ROOT;
	char line[CONFIG_LINE_MAX] = {0};
	for (unsigned int line_number = 1;
		fgets(line, sizeof(line), file) != NULL; line_number++) {
		if (strchr(line, '\n') == NULL && !feof(file)) {
			locky_log(LOCKY_ERROR, "config line %u is too long",
				line_number);
			int ch = 0;
			while ((ch = fgetc(file)) != '\n' && ch != EOF) {
			}
			if (ch == EOF) {
				break;
			}
			continue;
		}

		strip_comment(line);
		char *entry = trim(line);
		if (*entry == '\0') {
			continue;
		}
		if (entry[0] == '[') {
			section = parse_section(entry);
			if (section == SECTION_UNKNOWN) {
				locky_log(LOCKY_INFO,
					"unknown config section on line %u",
					line_number);
			}
			continue;
		}
		parse_entry(config, section, entry, line_number);
	}

	if (ferror(file)) {
		locky_log(LOCKY_ERROR, "failed while reading config file");
	}
	fclose(file);
	return true;
}

bool locky_config_load(struct locky_config *config) {
	char path[LOCKY_CONFIG_PATH_MAX] = {0};
	if (!locky_config_user_path(path, sizeof(path))) {
		return true;
	}
	return locky_config_load_path(config, path);
}
