#ifndef LOCKY_TIMER_H
#define LOCKY_TIMER_H

#include <stdbool.h>

bool locky_minute_timer_init(int *fd);
void locky_minute_timer_drain(int fd);
void locky_minute_timer_finish(int *fd);

#endif
