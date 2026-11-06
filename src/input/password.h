#ifndef SWEETLOCK_INPUT_PASSWORD_H
#define SWEETLOCK_INPUT_PASSWORD_H

#include <stdbool.h>
#include <stddef.h>

// Linux-PAM's PAM_MAX_RESP_SIZE, so PAM never truncates what was typed
enum { PASSWORD_MAX = 512 };

// lives in its own mlocked page; bytes[length] is always NUL
struct password {
	size_t length;
	char bytes[PASSWORD_MAX + 1];
};

struct password *password_create(void);

// wipes the bytes first, accepts NULL
void password_destroy(struct password *password);

// all or nothing: false, and nothing added, past PASSWORD_MAX
bool password_append(
	struct password *password, const char *text, size_t length);

// removes the last UTF-8 code point; false when already empty
bool password_backspace(struct password *password);

void password_clear(struct password *password);

#endif
