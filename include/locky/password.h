#ifndef LOCKY_PASSWORD_H
#define LOCKY_PASSWORD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <xkbcommon/xkbcommon.h>

struct locky_state;

struct locky_password {
	char *buffer;
	size_t len; // bytes used, excluding the trailing NUL
	size_t cap;
};

bool locky_password_init(struct locky_password *pw);
void locky_password_clear(struct locky_password *pw);
void locky_password_finish(struct locky_password *pw);

// Route a key press into the password buffer; never unlocks
void locky_keyboard_handle_key(
	struct locky_state *state, xkb_keysym_t keysym, uint32_t codepoint);

#endif
