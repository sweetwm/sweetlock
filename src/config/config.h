#ifndef SWEETLOCK_CONFIG_CONFIG_H
#define SWEETLOCK_CONFIG_CONFIG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
	CONFIG_MAX_BYTES = 64 * 1024,
	CONFIG_ERROR_SIZE = 256,
	CONFIG_NOTICE_SIZE = CONFIG_ERROR_SIZE + 32,
	CONFIG_STRING_SIZE = 128,
	CONFIG_PATH_SIZE = 4096,
	CONFIG_BLOBS_MAX = 3,
};

// background.layered.base takes the first three
enum background_mode {
	BACKGROUND_SOLID,
	BACKGROUND_GRADIENT,
	BACKGROUND_RADIAL,
	BACKGROUND_LAYERED,
	BACKGROUND_IMAGE,
};

enum background_fit {
	FIT_FILL,
	FIT_FIT,
	FIT_CENTER,
	FIT_STRETCH,
};

enum input_layout {
	LAYOUT_STATUS,
	LAYOUT_CORNER,
};

struct color {
	uint8_t red;
	uint8_t green;
	uint8_t blue;
	uint8_t alpha;
};

struct config_gradient {
	struct color top_left;
	struct color top_right;
	struct color bottom_left;
	struct color bottom_right;
};

// positions and sizes are percent of the output, so circles follow its shape
struct config_radial {
	struct color center;
	struct color edge;
	int32_t center_x;
	int32_t center_y;
	int32_t radius;
};

// fades from its colour, alpha included, to nothing
struct config_blob {
	struct color color;
	int32_t x;
	int32_t y;
	int32_t size;
};

struct config_image {
	char path[CONFIG_PATH_SIZE];
	enum background_fit fit;
	int32_t dim;
	int32_t blur;
};

struct config {
	struct {
		enum background_mode mode;
		struct color color;
		struct config_gradient gradient;
		struct config_radial radial;
		struct {
			enum background_mode base;
			int32_t blob_count;
			struct config_blob blob[CONFIG_BLOBS_MAX];
		} layered;
		struct config_image image;
	} background;
	struct {
		bool enabled;
		char format[CONFIG_STRING_SIZE];
	} clock;
	struct {
		bool enabled;
		char format[CONFIG_STRING_SIZE];
	} date;
	struct {
		bool enabled;
		// empty: the AccountsService icon, then ~/.face
		char path[CONFIG_PATH_SIZE];
	} avatar;
	struct {
		bool enabled;
		// empty: the GECOS full name, then the login name
		char text[CONFIG_STRING_SIZE];
	} username;
	struct {
		char placeholder[CONFIG_STRING_SIZE];
		bool reveal_toggle;
		enum input_layout layout;
	} input;
	struct {
		// empty, or naming no connected output: UI everywhere
		char output[CONFIG_STRING_SIZE];
	} ui;
	struct {
		// seconds without input before the outputs power off; 0 never
		int32_t timeout;
	} screen_off;
	struct {
		char font[CONFIG_STRING_SIZE];
		struct color foreground;
		struct color muted;
		struct color field;
		struct color error;
	} theme;
};

// one --set or alias, with key and value pointing into argv
struct config_override {
	const char *flag;
	const char *key;
	size_t key_length;
	const char *value;
};

void config_defaults(struct config *config);

bool config_parse(const char *text, size_t length, struct config *config,
	char *error, size_t error_size);

// applies one scalar key given as flag text, through the same schema
bool config_set(struct config *config, const char *key, size_t key_length,
	const char *value, char *error, size_t error_size);

bool config_load(struct config *config, const char *path, bool no_config,
	char notice[static CONFIG_NOTICE_SIZE]);

// applies each valid override and logs the rest; false if any was refused
bool config_override(struct config *config,
	const struct config_override *overrides, size_t count);

void config_log(const struct config *config);

#endif
