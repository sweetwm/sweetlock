#ifndef SWEETLOCK_AUTH_MESSAGE_H
#define SWEETLOCK_AUTH_MESSAGE_H

#include <stdbool.h>
#include <stddef.h>

#include "input/password.h"

enum {
	MESSAGE_REQUEST_MAX = PASSWORD_MAX + 1,
	MESSAGE_TEXT_MAX = 256,
	MESSAGE_REPLY_MAX = 1 + MESSAGE_TEXT_MAX,
	MESSAGE_TEXT_COUNT = 8,
	MESSAGE_SUCCESS = 'y',
	MESSAGE_FAILURE = 'n',
	MESSAGE_INFO = 'i',
	MESSAGE_ERROR = 'e',
};

enum message_kind {
	MESSAGE_KIND_SUCCESS,
	MESSAGE_KIND_FAILURE,
	MESSAGE_KIND_INFO,
	MESSAGE_KIND_ERROR,
};

struct message_reply {
	enum message_kind kind;
	// inside the packet, not terminated; empty for a verdict
	const char *text;
	size_t length;
};

// sets the password length; false for a packet that is not one request
bool message_request(const char *data, size_t size, size_t *length);

bool message_reply(const char *data, size_t size, struct message_reply *reply);

#endif
