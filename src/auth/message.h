#ifndef SWEETLOCK_AUTH_MESSAGE_H
#define SWEETLOCK_AUTH_MESSAGE_H

#include <stdbool.h>
#include <stddef.h>

#include "input/password.h"

// a request is the password and its NUL, so an empty one is still a packet;
// a reply is one byte
enum {
	MESSAGE_REQUEST_MAX = PASSWORD_MAX + 1,
	MESSAGE_REPLY_SIZE = 1,
	MESSAGE_SUCCESS = 'y',
	MESSAGE_FAILURE = 'n',
};

// sets the password length; false for a packet that is not one request
bool message_request(const char *data, size_t size, size_t *length);

bool message_reply(const char *data, size_t size, bool *success);

#endif
