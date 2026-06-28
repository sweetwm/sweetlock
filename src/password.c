#include "locky/password.h"
#include "locky/auth.h"
#include "locky/locky.h"
#include "locky/log.h"
#include "locky/render.h"
#include "locky/unicode.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define PASSWORD_CAP 1024

static size_t page_size(void) {
	long size = sysconf(_SC_PAGESIZE);
	return size > 0 ? (size_t)size : 4096;
}

bool locky_password_init(struct locky_password *pw) {
	void *buffer = NULL;
	int rc = posix_memalign(&buffer, page_size(), PASSWORD_CAP);
	if (rc != 0) {
		locky_log(LOCKY_ERROR, "failed to allocate password buffer");
		return false;
	}
	explicit_bzero(buffer, PASSWORD_CAP);

	if (mlock(buffer, PASSWORD_CAP) != 0 && errno != EPERM &&
		errno != ENOMEM) {
		locky_log(LOCKY_ERROR, "failed to mlock password buffer");
		free(buffer);
		return false;
	}

	pw->buffer = buffer;
	pw->cap = PASSWORD_CAP;
	pw->len = 0;
	return true;
}

void locky_password_clear(struct locky_password *pw) {
	if (pw->buffer != NULL) {
		explicit_bzero(pw->buffer, pw->cap);
	}
	pw->len = 0;
}

void locky_password_finish(struct locky_password *pw) {
	if (pw->buffer == NULL) {
		return;
	}
	explicit_bzero(pw->buffer, pw->cap);
	munlock(pw->buffer, pw->cap); // no-op when mlock was denied
	free(pw->buffer);
	pw->buffer = NULL;
	pw->cap = 0;
	pw->len = 0;
}

size_t locky_password_length(const struct locky_password *pw) {
	size_t count = 0;
	for (size_t i = 0; i < pw->len; i++) {
		unsigned char ch = (unsigned char)pw->buffer[i];
		if ((ch & 0xC0) != 0x80) {
			count++;
		}
	}
	return count;
}

static void password_append(struct locky_password *pw, uint32_t codepoint) {
	size_t size = utf8_chsize(codepoint);
	if (pw->len + size + 1 > pw->cap) { // keep room for the trailing NUL
		locky_log(LOCKY_ERROR, "password buffer full");
		return;
	}
	utf8_encode(&pw->buffer[pw->len], codepoint);
	pw->len += size;
	pw->buffer[pw->len] = '\0';
}

static void password_backspace(struct locky_password *pw) {
	if (pw->len == 0) {
		return;
	}
	size_t old_len = pw->len;
	pw->len -= (size_t)utf8_last_size(pw->buffer);
	explicit_bzero(&pw->buffer[pw->len], old_len - pw->len + 1);
}

void locky_keyboard_handle_key(
	struct locky_state *state, xkb_keysym_t keysym, uint32_t codepoint) {
	struct locky_password *pw = &state->password;

	// Ctrl shortcuts clear the buffer
	if (state->xkb.control) {
		if (keysym == XKB_KEY_u || keysym == XKB_KEY_c) {
			locky_password_clear(pw);
			locky_render_all_outputs(state);
		}
		return; // never feed control combos into the password
	}

	switch (keysym) {
	case XKB_KEY_KP_Enter:
	case XKB_KEY_Return:
		locky_auth_submit(state);
		break;
	case XKB_KEY_BackSpace:
	case XKB_KEY_Delete:
		password_backspace(pw);
		break;
	case XKB_KEY_Escape:
		locky_password_clear(pw);
		break;
	default:
		if (codepoint != 0) {
			if (state->auth_state == LOCKY_AUTH_INVALID) {
				state->auth_state = LOCKY_AUTH_IDLE;
			}
			password_append(pw, codepoint);
		}
		break;
	}
	locky_render_all_outputs(state);
}
