#ifndef SWEETLOCK_APP_SIGNALS_H
#define SWEETLOCK_APP_SIGNALS_H

#include <stdbool.h>

enum signal_action {
	SIGNAL_NONE,
	SIGNAL_UNLOCK,
	SIGNAL_EXIT,
};

// the signals app_protect blocked, as a non-blocking signalfd
bool signals_open(int *fd);

enum signal_action signals_read(int fd);

#endif
