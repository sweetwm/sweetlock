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
	if (size == 0 || size > MESSAGE_REPLY_MAX) {
		return false;
	}
	*reply = (struct message_reply){.text = data + 1};
	if (size == 1) {
		reply->kind = data[0] == MESSAGE_SUCCESS ? MESSAGE_KIND_SUCCESS
							 : MESSAGE_KIND_FAILURE;
		return data[0] == MESSAGE_SUCCESS || data[0] == MESSAGE_FAILURE;
	}
	reply->kind = data[0] == MESSAGE_INFO ? MESSAGE_KIND_INFO
					      : MESSAGE_KIND_ERROR;
	reply->length = size - 1;
	return (data[0] == MESSAGE_INFO || data[0] == MESSAGE_ERROR) &&
	       utf8_is_plain(reply->text, reply->length);
}
