#include "app/time.h"
#include <time.h>

uint64_t locky_time_msec(void) {
	struct timespec ts = {0};
	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
		return 0;
	}
	return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}
