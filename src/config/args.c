#include "config/args.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "wayland/scale.h"

const char args_usage[] =
	"Sweetlock - a screen locker for Wayland\n"
	"\n"
	"Usage: sweetlock [OPTIONS]\n"
	"\n"
	"Options:\n"
	"  --config PATH     Read the config from PATH\n"
	"  --no-config       Ignore the config file\n"
	"  --set KEY=VALUE   Set any config key, e.g. "
	"background.color=#000000\n"
	"  --color HEX       Same as --set background.color=HEX\n"
	"  --image PATH      Same as --set background.mode=image\n"
	"                    --set background.image.path=PATH\n"
	"  --fit MODE        Same as --set background.image.fit=MODE\n"
	"  --dim N           Same as --set background.image.dim=N\n"
	"  --blur N          Same as --set background.image.blur=N\n"
	"  --no-clock        Same as --set clock.enabled=false\n"
	"  --no-date         Same as --set date.enabled=false\n"
	"  --no-avatar       Same as --set avatar.enabled=false\n"
	"  --no-username     Same as --set username.enabled=false\n"
	"  -f, --daemonize   Return once the screen is locked\n"
	"  --check           Report problems with the config, the PAM service\n"
	"                    and the images, then exit without locking\n"
	"  --preview FILE    Draw the lock screen to a PNG instead of locking\n"
	"  --size WxH        Output size in pixels for --preview "
	"(default 1920x1080)\n"
	"  --scale S         Output scale for --preview (default 1)\n"
	"  --preview-state STATE\n"
	"                    Password field for --preview: idle, typing, "
	"revealed,\n"
	"                    checking, failed, info, error, caps or layout\n"
	"                    (default idle)\n"
	"  -h, --help        Print help\n"
	"  -V, --version     Print version\n";

struct alias {
	const char *flag;
	const char *key;
	// set for a flag that takes no value
	const char *value;
};

static const struct alias aliases[] = {
	{"--color", "background.color", NULL},
	{"--image", "background.image.path", NULL},
	{"--fit", "background.image.fit", NULL},
	{"--dim", "background.image.dim", NULL},
	{"--blur", "background.image.blur", NULL},
	{"--no-clock", "clock.enabled", "false"},
	{"--no-date", "date.enabled", "false"},
	{"--no-avatar", "avatar.enabled", "false"},
	{"--no-username", "username.enabled", "false"},
};

static bool is(const char *arg, const char *short_name, const char *name) {
	return strcmp(arg, short_name) == 0 || strcmp(arg, name) == 0;
}

static const struct alias *find_alias(const char *flag) {
	for (size_t i = 0; i < sizeof aliases / sizeof aliases[0]; i++) {
		if (strcmp(flag, aliases[i].flag) == 0) {
			return &aliases[i];
		}
	}
	return NULL;
}

static void add(struct args *args, const char *flag, const char *key,
	size_t key_length, const char *value) {
	if (args->override_count == ARGS_OVERRIDE_MAX) {
		log_error("%s: more than %d config flags; ignoring it", flag,
			ARGS_OVERRIDE_MAX);
		return;
	}
	args->overrides[args->override_count++] = (struct config_override){
		.flag = flag,
		.key = key,
		.key_length = key_length,
		.value = value,
	};
}

static void set(struct args *args, const char *flag, const char *value) {
	const char *equals = strchr(value, '=');
	if (!equals || equals == value) {
		log_error("%s: expected KEY=VALUE; ignoring it", flag);
		return;
	}
	add(args, flag, value, (size_t)(equals - value), equals + 1);
}

static bool parse_side(
	const char *text, char end, const char **rest, int32_t *out) {
	if (*text < '0' || *text > '9') {
		return false;
	}
	char *stop = NULL;
	errno = 0;
	long value = strtol(text, &stop, 10);
	if (errno != 0 || *stop != end || value < 1 ||
		value > ARGS_PREVIEW_SIZE_MAX) {
		return false;
	}
	*out = (int32_t)value;
	*rest = stop + 1;
	return true;
}

static void parse_size(struct args *args, const char *flag, const char *text) {
	int32_t width = 0;
	int32_t height = 0;
	const char *rest = text;
	if (!parse_side(rest, 'x', &rest, &width) ||
		!parse_side(rest, '\0', &rest, &height)) {
		log_error("%s: expected WxH, each 1 to %d; ignoring it", flag,
			ARGS_PREVIEW_SIZE_MAX);
		return;
	}
	args->preview.width = width;
	args->preview.height = height;
}

static void parse_scale(struct args *args, const char *flag, const char *text) {
	char *stop = NULL;
	errno = 0;
	double value = strtod(text, &stop);
	double scaled = value * (double)SCALE_DENOMINATOR;
	// a scale that rounds to 0/120 is no scale at all
	if (errno != 0 || stop == text || *stop != '\0' ||
		!(scaled >= 0.5 && scaled <= (double)SCALE_MAX)) {
		log_error("%s: expected a number above 0 and at most %d; "
			  "ignoring it",
			flag, SCALE_MAX / SCALE_DENOMINATOR);
		return;
	}
	args->preview.scale = (uint32_t)(scaled + 0.5);
}

static void parse_state(struct args *args, const char *flag, const char *text) {
	static const char *const names[] = {
		[PREVIEW_IDLE] = "idle",
		[PREVIEW_TYPING] = "typing",
		[PREVIEW_REVEALED] = "revealed",
		[PREVIEW_CHECKING] = "checking",
		[PREVIEW_FAILED] = "failed",
		[PREVIEW_INFO] = "info",
		[PREVIEW_ERROR] = "error",
		[PREVIEW_CAPS] = "caps",
		[PREVIEW_LAYOUT] = "layout",
	};
	for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) {
		if (strcmp(text, names[i]) == 0) {
			args->preview.field = (enum preview_field)i;
			return;
		}
	}
	log_error("%s: expected idle, typing, revealed, checking, failed, "
		  "info, error, caps or layout; ignoring it",
		flag);
}

// flag is one that takes_value accepts
static void parse_valued(
	struct args *args, const char *flag, const char *value) {
	if (strcmp(flag, "--config") == 0) {
		args->config_path = value;
	} else if (strcmp(flag, "--set") == 0) {
		set(args, flag, value);
	} else if (strcmp(flag, "--preview") == 0) {
		args->preview.path = value;
	} else if (strcmp(flag, "--size") == 0) {
		parse_size(args, flag, value);
	} else if (strcmp(flag, "--scale") == 0) {
		parse_scale(args, flag, value);
	} else if (strcmp(flag, "--preview-state") == 0) {
		parse_state(args, flag, value);
	} else {
		const char *key = find_alias(flag)->key;
		// a path alone would not show the image
		if (strcmp(flag, "--image") == 0) {
			add(args, flag, "background.mode",
				strlen("background.mode"), "image");
		}
		add(args, flag, key, strlen(key), value);
	}
}

static bool takes_value(const char *flag) {
	static const char *const flags[] = {"--config", "--set", "--preview",
		"--size", "--scale", "--preview-state"};
	for (size_t i = 0; i < sizeof flags / sizeof flags[0]; i++) {
		if (strcmp(flag, flags[i]) == 0) {
			return true;
		}
	}
	const struct alias *alias = find_alias(flag);
	return alias && !alias->value;
}

void args_parse(struct args *args, int argc, char **argv) {
	*args = (struct args){
		.action = ARGS_RUN,
		.preview =
			{
				.width = 1920,
				.height = 1080,
				.scale = SCALE_DENOMINATOR,
			},
	};
	bool check = false;
	for (int i = 1; i < argc; i++) {
		const char *arg = argv[i];
		const struct alias *alias = find_alias(arg);
		if (is(arg, "-h", "--help")) {
			args->action = ARGS_HELP;
			return;
		}
		if (is(arg, "-V", "--version")) {
			args->action = ARGS_VERSION;
			return;
		}
		if (is(arg, "-f", "--daemonize")) {
			args->daemonize = true;
		} else if (strcmp(arg, "--check") == 0) {
			check = true;
		} else if (strcmp(arg, "--no-config") == 0) {
			args->no_config = true;
		} else if (alias && alias->value) {
			add(args, arg, alias->key, strlen(alias->key),
				alias->value);
		} else if (!takes_value(arg)) {
			// a bad flag must never stop the screen from locking
			log_error("ignoring unknown argument \"%s\"", arg);
		} else if (i + 1 == argc) {
			log_error("%s: missing its value; ignoring it", arg);
		} else {
			parse_valued(args, arg, argv[++i]);
		}
	}
	if (check) {
		args->action = ARGS_CHECK;
	} else if (args->preview.path) {
		args->action = ARGS_PREVIEW;
	}
}
