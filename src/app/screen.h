#ifndef SWEETLOCK_APP_SCREEN_H
#define SWEETLOCK_APP_SCREEN_H

#include <stdbool.h>
#include <stdint.h>

struct display;
struct input;

// input only stamps a time, so pointer motion costs no syscall
struct screen {
	int fd;
	// 0 turns either off
	int64_t hide_ns;
	int64_t off_ns;
};

bool screen_init(struct screen *screen, int32_t hide_s, int32_t off_s);

void screen_finish(struct screen *screen);

// after locked; screens go off only if the compositor offers power control
void screen_start(
	struct screen *screen, struct display *display, struct input *input);

// fd is readable; held keeps everything as it is, as an attempt does
void screen_expire(struct screen *screen, struct display *display,
	struct input *input, bool held);

// a key or click while some output is off
void screen_wake(
	struct screen *screen, struct display *display, struct input *input);

// the login part came back, so its hide is due again
void screen_show(struct screen *screen, const struct input *input);

#endif
