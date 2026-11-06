#include "auth/message.h"

#include <string.h>

// PAM takes the password as a C string, so a NUL inside it would cut it short
bool message_request(const char *data, size_t size, size_t *length) {
	if (size == 0 || size > MESSAGE_REQUEST_MAX || data[size - 1] != '\0' ||
		memchr(data, '\0', size - 1)) {
		return false;
	}
	*length = size - 1;
	return true;
}

bool message_reply(const char *data, size_t size, bool *success) {
	if (size != MESSAGE_REPLY_SIZE ||
		(data[0] != MESSAGE_SUCCESS && data[0] != MESSAGE_FAILURE)) {
		return false;
	}
	*success = data[0] == MESSAGE_SUCCESS;
	return true;
}
