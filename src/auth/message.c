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

size_t message_text(
	const char *text, size_t size, char out[static MESSAGE_TEXT_MAX]) {
	size_t length = 0;
	size_t i = 0;
	while (i < size) {
		size_t step = utf8_plain_sequence(text + i, size - i);
		if (step == 0) {
			// a space is plain, and a leading one is dropped below
			if (length > 0 && length < MESSAGE_TEXT_MAX) {
				out[length++] = ' ';
			}
			i++;
			continue;
		}
		if (length + step > MESSAGE_TEXT_MAX) {
			break;
		}
		if (length > 0 || text[i] != ' ') {
			memcpy(out + length, text + i, step);
			length += step;
		}
		i += step;
	}
	while (length > 0 && out[length - 1] == ' ') {
		length--;
	}
	return length;
}
