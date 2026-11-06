#include <stdio.h>
#include <stdlib.h>

#include "args.h"
#include "config.h"
#include "log.h"

int main(int argc, char **argv) {
	bool log_level_ok = log_init();
	struct args args;
	args_parse(&args, argc, argv);
	if (args.action == ARGS_HELP) {
		fputs(args_usage, stdout);
		return EXIT_SUCCESS;
	}
	if (args.action == ARGS_VERSION) {
		puts("sweetlock " SWEETLOCK_VERSION);
		return EXIT_SUCCESS;
	}

	if (!log_level_ok) {
		log_warn("SWEETLOCK_LOG is not error, warn, info or debug; "
			 "using info");
	}
	log_info("starting Sweetlock version=%s", SWEETLOCK_VERSION);
	struct config config;
	config_load(&config, args.config_path, args.no_config);
	config_override(&config, args.overrides, args.override_count);
	config_log(&config);

	log_error("locking is not implemented yet");
	return EXIT_FAILURE;
}
