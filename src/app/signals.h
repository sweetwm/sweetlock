#ifndef SWEETLOCK_APP_SIGNALS_H
#define SWEETLOCK_APP_SIGNALS_H

#include <stdbool.h>

enum signal_action {
	SIGNAL_NONE,
	SIGNAL_UNLOCK,
	SIGNAL_EXIT,
};

// the signals app_protect blocked, as a descriptor for poll: a signalfd,
// or a kqueue on FreeBSD
bool signals_open(int *fd);

enum signal_action signals_read(int fd);

#endif
