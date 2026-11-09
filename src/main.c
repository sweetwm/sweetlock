#include <fcntl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "app/app.h"
#include "auth/helper.h"
#include "config/args.h"
#include "config/config.h"
#include "log.h"

// a closed stdio number goes to the next socket we open, and the log with it
static bool stdio_open(void) {
	for (int fd = STDIN_FILENO; fd <= STDERR_FILENO; fd++) {
		if (fcntl(fd, F_GETFD) < 0 && open("/dev/null", O_RDWR) != fd) {
			return false;
		}
	}
	return true;
}

int main(int argc, char **argv) {
	struct timespec start = {0};
	clock_gettime(CLOCK_MONOTONIC, &start);
	if (!stdio_open()) {
		return EXIT_FAILURE;
	}
	bool log_level_ok = log_init();
	if (!app_protect()) {
		return EXIT_FAILURE;
	}
	// the locker starts its PAM helper as itself, never by hand
	if (argc == 3 && strcmp(argv[1], HELPER_FLAG) == 0) {
		helper_main(argv[2]);
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
	char notice[CONFIG_NOTICE_SIZE];
	bool config_ok =
		config_load(&config, args.config_path, args.no_config, notice);
	config_ok &=
		config_override(&config, args.overrides, args.override_count);
	config_log(&config);
	if (args.action == ARGS_CHECK) {
		return app_check(&config, config_ok);
	}
	// only LC_TIME: a different LC_NUMERIC would change how numbers parse
	setlocale(LC_TIME, "");
	if (args.action == ARGS_PREVIEW) {
		return app_preview(&config, notice, &args.preview);
	}
	// -f forks here, before the PAM helper and any library thread
	int locked_fd = args.daemonize ? app_daemonize() : -1;
	return app_run(&config, notice, &start, locked_fd);
}
