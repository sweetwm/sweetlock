#include "auth/message.h"

#include <string.h>

#include "utf8.h"

// PAM takes the password as a C string, so a NUL inside it would cut it short
bool message_request(const char *data, size_t size, size_t *length) {
	if (size == 0 || size > MESSAGE_REQUEST_MAX || data[size - 1] != '\0' ||
		memchr(data, '\0', size - 1)) {
		return false;
	}
	*length = size - 1;
	return true;
}

bool message_reply(const char *data, size_t size, struct message_reply *reply) {
	*reply = (struct message_reply){0};
	if (size == 0 || size > MESSAGE_REPLY_MAX) {
		return false;
	}
	char kind = data[0];
	bool valid =
		size == 1 ? kind == MESSAGE_SUCCESS || kind == MESSAGE_FAILURE
			  : (kind == MESSAGE_INFO || kind == MESSAGE_ERROR) &&
				    utf8_is_plain(data + 1, size - 1);
	if (valid) {
		*reply = (struct message_reply){
			.kind = kind, .text = data + 1, .length = size - 1};
	}
	return valid;
}
