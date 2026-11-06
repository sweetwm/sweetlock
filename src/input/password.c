#include "input/password.h"

#include <string.h>
#include <sys/mman.h>

#include "log.h"

struct password *password_create(void) {
	struct password *password = mmap(NULL, sizeof *password,
		PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (password == MAP_FAILED) {
		log_error("failed to allocate the password buffer: %m");
		return NULL;
	}
	// typing still works without it; the password could reach swap
	if (mlock(password, sizeof *password) != 0) {
		log_warn("failed to lock the password buffer in memory: %m");
	}
	return password;
}

void password_destroy(struct password *password) {
	if (!password) {
		return;
	}
	explicit_bzero(password, sizeof *password);
	munmap(password, sizeof *password);
}

bool password_append(
	struct password *password, const char *text, size_t length) {
	if (length > PASSWORD_MAX - password->length) {
		return false;
	}
	memcpy(password->bytes + password->length, text, length);
	password->length += length;
	password->bytes[password->length] = '\0';
	return true;
}

static bool is_continuation(char byte) {
	return ((unsigned char)byte & 0xc0) == 0x80;
}

bool password_backspace(struct password *password) {
	if (password->length == 0) {
		return false;
	}
	size_t end = password->length - 1;
	while (end > 0 && is_continuation(password->bytes[end])) {
		end--;
	}
	explicit_bzero(password->bytes + end, password->length - end);
	password->length = end;
	return true;
}

void password_clear(struct password *password) {
	explicit_bzero(password->bytes, sizeof password->bytes);
	password->length = 0;
}
