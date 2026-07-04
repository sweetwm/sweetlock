#ifndef LOCKY_CONFIG_H
#define LOCKY_CONFIG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LOCKY_CONFIG_PATH_MAX 4096

struct locky_config {
	uint32_t background_color;
	bool has_background_image;
	char background_image[LOCKY_CONFIG_PATH_MAX];
	bool has_avatar_image;
	char avatar_image[LOCKY_CONFIG_PATH_MAX];
};

void locky_config_defaults(struct locky_config *config);
bool locky_config_user_path(char *path, size_t path_size);
bool locky_config_load(struct locky_config *config);
bool locky_config_load_path(struct locky_config *config, const char *path);

#endif
