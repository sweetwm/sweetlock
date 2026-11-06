#ifndef SWEETLOCK_INPUT_INPUT_H
#define SWEETLOCK_INPUT_INPUT_H

#include <stdbool.h>
#include <stdint.h>

struct auth;
struct keyboard;
struct password;
struct xkb_context;

// what every keyboard shares: the password, the attempt and key repeat
struct input {
	struct auth *auth;
	struct xkb_context *xkb;
	struct password *password;
	int repeat_fd;
	// the held key that repeats, if any
	struct keyboard *repeat_keyboard;
	uint32_t repeat_key;
};

bool input_init(struct input *input, struct auth *auth);

void input_finish(struct input *input);

void input_press(struct input *input, struct keyboard *keyboard, uint32_t key);

void input_release(
	struct input *input, const struct keyboard *keyboard, uint32_t key);

// stops a repeat from this keyboard: on leave, a new keymap or its destroy
void input_forget(struct input *input, const struct keyboard *keyboard);

// the repeat timer is readable
void input_repeat(struct input *input);

#endif
