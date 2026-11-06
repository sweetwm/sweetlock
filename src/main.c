#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "app.h"
#include "args.h"
#include "config.h"
#include "log.h"

int main(int argc, char **argv) {
	struct timespec start = {0};
	clock_gettime(CLOCK_MONOTONIC, &start);
	bool log_level_ok = log_init();
	if (!app_protect()) {
		return EXIT_FAILURE;
	}
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
	// only LC_TIME: a different LC_NUMERIC would change how numbers parse
	setlocale(LC_TIME, "");
	if (args.action == ARGS_PREVIEW) {
		return app_preview(&config, &args.preview);
	}
	// -f forks here, before the PAM helper and any library thread
	int locked_fd = args.daemonize ? app_daemonize() : -1;
	return app_run(&config, &start, locked_fd);
}
