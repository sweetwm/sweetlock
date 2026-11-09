#ifndef SWEETLOCK_INPUT_KEYBOARD_H
#define SWEETLOCK_INPUT_KEYBOARD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "input/key.h"

struct input;
struct wl_keyboard;

// one wl_keyboard with its keymap and xkb state
struct keyboard;

// takes ownership of wl, released on failure
struct keyboard *keyboard_create(struct input *input, struct wl_keyboard *wl);

void keyboard_destroy(struct keyboard *keyboard);

// key_translate with this keyboard's state; key is an evdev code
enum key_action keyboard_translate(
	struct keyboard *keyboard, uint32_t key, char *text, size_t size);

// false when the key does not repeat
bool keyboard_repeats(const struct keyboard *keyboard, uint32_t key,
	int32_t *rate, int32_t *delay);

#endif
