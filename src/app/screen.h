#ifndef SWEETLOCK_APP_SCREEN_H
#define SWEETLOCK_APP_SCREEN_H

#include <stdbool.h>
#include <stdint.h>

struct display;
struct input;

// input only stamps a time, so pointer motion costs no syscall
struct screen {
	int fd;
	int64_t timeout_ns;
};

bool screen_init(struct screen *screen, int32_t timeout_s);

void screen_finish(struct screen *screen);

// after locked; the timer runs only if the compositor offers power control
void screen_start(
	struct screen *screen, struct display *display, struct input *input);

// fd is readable, the screens go off once input has been idle long enough
void screen_expire(struct screen *screen, struct display *display,
	struct input *input, bool busy);

// a key or click while some output is off
void screen_wake(struct screen *screen, struct display *display);

#endif
