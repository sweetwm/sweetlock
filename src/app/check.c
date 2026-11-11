#include "app/app.h"

#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>

#include "auth/service.h"
#include "fs/file.h"
#include "fs/path.h"
#include "image/image.h"
#include "input/layout.h"
#include "log.h"

// the same reads and header checks the lock does, without decoding
static bool image_ok(const char *key, const char *configured) {
	char path[PATH_MAX];
	const char *why = NULL;
	if (!path_expand(path, configured, &why)) {
		log_error("%s: %s", key, why);
		return false;
	}
	if (!path[0]) {
		log_error("%s is empty", key);
		return false;
	}
	char error[256] = "";
	char *data = NULL;
	size_t length = 0;
	enum file_result result = file_read(
		path, IMAGE_FILE_MAX, &data, &length, error, sizeof error);
	uint32_t width = 0;
	uint32_t height = 0;
	enum image_format format =
		result == FILE_OK ? image_probe((const unsigned char *)data,
					    length, &width, &height)
				  : IMAGE_UNKNOWN;
	free(data);
	if (result == FILE_MISSING) {
		log_error("%s: %s: no such file", key, path);
	} else if (result != FILE_OK) {
		log_error("%s: %s: %s", key, path, error);
	} else if (format == IMAGE_UNKNOWN) {
		log_error("%s: %s: not a PNG or JPEG image", key, path);
	} else if (!image_fits(width, height)) {
		log_error("%s: %s: %" PRIu32 "x%" PRIu32
			  " is past the limit of %d pixels",
			key, path, width, height, IMAGE_PIXELS_MAX);
	} else {
		return true;
	}
	return false;
}

// a missing list only costs the badge its exact codes, so it warns
static void layouts_ok(void) {
	char path[PATH_MAX];
	char error[256] = "";
	char *data = NULL;
	size_t length = 0;
	enum file_result result =
		layout_list_load(path, &data, &length, error, sizeof error);
	free(data);
	if (result == FILE_OK) {
		log_info("input.layout: layout codes from %s", path);
	} else if (result == FILE_MISSING) {
		log_warn("input.layout: no xkeyboard-config rules/evdev.lst");
	} else {
		log_warn("input.layout: %s: %s", path, error);
	}
}

int app_check(const struct config *config, bool config_ok) {
	bool ok = config_ok;
	if (!service_found()) {
		log_error(SERVICE_MISSING_LOG);
		ok = false;
	}
	if (config->avatar.enabled && config->avatar.path[0] &&
		!image_ok("avatar.path", config->avatar.path)) {
		ok = false;
	}
	if (config->background.mode == BACKGROUND_IMAGE &&
		!image_ok("background.image.path",
			config->background.image.path)) {
		ok = false;
	}
	if (config->input.layout == LAYOUT_CORNER) {
		layouts_ok();
	}
	if (!ok) {
		log_error("--check found problems");
		return EXIT_FAILURE;
	}
	log_info("--check found no problems");
	return EXIT_SUCCESS;
}
