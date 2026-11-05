#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"

static const char usage[] = "Sweetlock - a screen locker for Wayland\n"
			    "\n"
			    "Usage: sweetlock [OPTIONS]\n"
			    "\n"
			    "Options:\n"
			    "  -h, --help     Print help\n"
			    "  -V, --version  Print version\n";

static bool is(const char *arg, const char *short_name, const char *name) {
	return strcmp(arg, short_name) == 0 || strcmp(arg, name) == 0;
}

int main(int argc, char **argv) {
	bool log_level_ok = log_init();
	for (int i = 1; i < argc; i++) {
		if (is(argv[i], "-h", "--help")) {
			fputs(usage, stdout);
			return EXIT_SUCCESS;
		}
		if (is(argv[i], "-V", "--version")) {
			puts("sweetlock " SWEETLOCK_VERSION);
			return EXIT_SUCCESS;
		}
		// a bad flag must never stop the screen from locking
		log_error("ignoring unknown argument \"%s\"", argv[i]);
	}

	if (!log_level_ok) {
		log_warn("SWEETLOCK_LOG is not error, warn, info or debug; "
			 "using info");
	}
	log_info("starting Sweetlock version=%s", SWEETLOCK_VERSION);
	log_error("locking is not implemented yet");
	return EXIT_FAILURE;
}
