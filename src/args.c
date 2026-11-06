#include "args.h"

#include <string.h>

#include "log.h"

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
	"  -f, --daemonize   Return once the screen is locked\n"
	"  -h, --help        Print help\n"
	"  -V, --version     Print version\n";

struct alias {
	const char *flag;
	const char *key;
};

static const struct alias aliases[] = {
	{"--color", "background.color"},
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

void args_parse(struct args *args, int argc, char **argv) {
	*args = (struct args){.action = ARGS_RUN};
	for (int i = 1; i < argc; i++) {
		const char *arg = argv[i];
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
			continue;
		}
		if (strcmp(arg, "--no-config") == 0) {
			args->no_config = true;
			continue;
		}
		bool config = strcmp(arg, "--config") == 0;
		bool set_key = strcmp(arg, "--set") == 0;
		const struct alias *alias = find_alias(arg);
		// a bad flag must never stop the screen from locking
		if (!config && !set_key && !alias) {
			log_error("ignoring unknown argument \"%s\"", arg);
			continue;
		}
		if (i + 1 == argc) {
			log_error("%s: missing its value; ignoring it", arg);
			continue;
		}
		const char *value = argv[++i];
		if (config) {
			args->config_path = value;
		} else if (set_key) {
			set(args, arg, value);
		} else {
			add(args, arg, alias->key, strlen(alias->key), value);
		}
	}
}
