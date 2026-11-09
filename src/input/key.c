#include "input/key.h"

#include <stdbool.h>
#include <string.h>

// control characters, Ctrl+letter among them, never go into a password
static bool is_text(const char *text, size_t length) {
	for (size_t i = 0; i < length; i++) {
		unsigned char byte = (unsigned char)text[i];
		if (byte < 0x20 || byte == 0x7f) {
			return false;
		}
	}
	return length > 0;
}

static enum key_action take(
	enum key_action action, int length, char *text, size_t size) {
	if (length < 0 || (size_t)length >= size ||
		!is_text(text, (size_t)length)) {
		explicit_bzero(text, size);
		return KEY_NONE;
	}
	return action;
}

// a key that ends or breaks a sequence is used up by it, as in other
// xkbcommon clients
static bool composes(struct xkb_compose_state *compose, xkb_keysym_t sym,
	enum key_action *action, char *text, size_t size) {
	if (!compose || xkb_compose_state_feed(compose, sym) !=
				XKB_COMPOSE_FEED_ACCEPTED) {
		return false;
	}
	enum xkb_compose_status status = xkb_compose_state_get_status(compose);
	if (status == XKB_COMPOSE_NOTHING) {
		return false;
	}
	*action = KEY_NONE;
	if (status == XKB_COMPOSE_COMPOSED) {
		int length = xkb_compose_state_get_utf8(compose, text, size);
		*action = take(KEY_COMPOSED, length, text, size);
	}
	if (status != XKB_COMPOSE_COMPOSING) {
		xkb_compose_state_reset(compose);
	}
	return true;
}

enum key_action key_translate(struct xkb_state *state,
	struct xkb_compose_state *compose, xkb_keycode_t code, char *text,
	size_t size) {
	xkb_keysym_t sym = xkb_state_key_get_one_sym(state, code);
	bool ctrl = xkb_state_mod_name_is_active(state, XKB_MOD_NAME_CTRL,
			    XKB_STATE_MODS_EFFECTIVE) > 0;
	enum key_action action = KEY_NONE;
	if (sym == XKB_KEY_Return || sym == XKB_KEY_KP_Enter) {
		action = KEY_SUBMIT;
	} else if (sym == XKB_KEY_Escape ||
		   (ctrl && (sym == XKB_KEY_u || sym == XKB_KEY_U))) {
		action = KEY_CLEAR;
	}
	if (action != KEY_NONE) {
		if (compose) {
			xkb_compose_state_reset(compose);
		}
		return action;
	}
	// Backspace cancels a pending dead key instead of deleting
	if (composes(compose, sym, &action, text, size)) {
		return action;
	}
	if (sym == XKB_KEY_BackSpace) {
		return KEY_BACKSPACE;
	}
	int length = xkb_state_key_get_utf8(state, code, text, size);
	return take(KEY_TEXT, length, text, size);
}
