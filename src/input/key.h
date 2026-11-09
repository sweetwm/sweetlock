#ifndef SWEETLOCK_INPUT_KEY_H
#define SWEETLOCK_INPUT_KEY_H

#include <stdbool.h>
#include <stddef.h>
#include <xkbcommon/xkbcommon-compose.h>
#include <xkbcommon/xkbcommon.h>

enum key_action {
	KEY_NONE,
	KEY_TEXT,
	// text from a dead key or Compose sequence, which never repeats
	KEY_COMPOSED,
	KEY_BACKSPACE,
	KEY_CLEAR,
	KEY_SUBMIT,
};

enum { KEY_TEXT_SIZE = 64 };

enum key_action key_translate(struct xkb_state *state,
	struct xkb_compose_state *compose, xkb_keycode_t code, char *text,
	size_t size);

// layout is empty unless the keymap has more than one
bool key_status(struct xkb_state *state, char *layout, size_t size);

#endif
