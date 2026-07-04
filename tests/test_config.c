#include "app/log.h"
#include "config/config.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void write_config_file(const char *path, const char *contents) {
	FILE *file = fopen(path, "w");
	assert(file != NULL);
	assert(fputs(contents, file) >= 0);
	assert(fclose(file) == 0);
}

static void test_defaults(void) {
	struct locky_config config = {0};
	locky_config_defaults(&config);

	assert(config.background_color == 0x1E1E2E);
	assert(!config.has_background_image);
	assert(!config.has_avatar_image);
}

static void test_user_path_prefers_xdg_config_home(void) {
	char path[LOCKY_CONFIG_PATH_MAX] = {0};

	assert(setenv("XDG_CONFIG_HOME", "/tmp/locky-xdg", 1) == 0);
	assert(setenv("HOME", "/tmp/locky-home", 1) == 0);
	assert(locky_config_user_path(path, sizeof(path)));
	assert(strcmp(path, "/tmp/locky-xdg/locky/config.toml") == 0);

	assert(unsetenv("XDG_CONFIG_HOME") == 0);
	assert(locky_config_user_path(path, sizeof(path)));
	assert(strcmp(path, "/tmp/locky-home/.config/locky/config.toml") == 0);
}

static void test_load_valid_config(void) {
	char path[] = "/tmp/locky-config-test-XXXXXX";
	int fd = mkstemp(path);
	assert(fd >= 0);
	close(fd);

	assert(setenv("HOME", "/home/locky-test", 1) == 0);
	write_config_file(path, "[background]\n"
				"color = \"#112233\"\n"
				"image = \"~/Pictures/wall.png\"\n"
				"\n"
				"[user]\n"
				"avatar = '/tmp/avatar.png'\n");

	struct locky_config config = {0};
	locky_config_defaults(&config);
	assert(locky_config_load_path(&config, path));
	assert(config.background_color == 0x112233);
	assert(config.has_background_image);
	assert(strcmp(config.background_image,
		       "/home/locky-test/Pictures/wall.png") == 0);
	assert(config.has_avatar_image);
	assert(strcmp(config.avatar_image, "/tmp/avatar.png") == 0);

	unlink(path);
}

static void test_invalid_values_keep_defaults(void) {
	char path[] = "/tmp/locky-config-test-XXXXXX";
	int fd = mkstemp(path);
	assert(fd >= 0);
	close(fd);

	write_config_file(path, "[background]\n"
				"color = \"oops\"\n"
				"image = /tmp/nope.png\n"
				"[user]\n"
				"avatar = /tmp/nope.png\n");

	struct locky_config config = {0};
	locky_config_defaults(&config);
	assert(locky_config_load_path(&config, path));
	assert(config.background_color == 0x1E1E2E);
	assert(!config.has_background_image);
	assert(!config.has_avatar_image);

	unlink(path);
}

static void test_missing_file_keeps_defaults(void) {
	struct locky_config config = {0};
	locky_config_defaults(&config);
	assert(locky_config_load_path(&config, "/tmp/locky-missing-config"));
	assert(config.background_color == 0x1E1E2E);
}

int main(void) {
	locky_log_init(LOCKY_SILENT);

	test_defaults();
	test_user_path_prefers_xdg_config_home();
	test_load_valid_config();
	test_invalid_values_keep_defaults();
	test_missing_file_keeps_defaults();
	return 0;
}
