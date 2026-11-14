#ifndef SWEETLOCK_AUTH_MESSAGE_H
#define SWEETLOCK_AUTH_MESSAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <sys/socket.h>

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
	// FreeBSD ends a SOCK_SEQPACKET record only at MSG_EOR and merges sends
	// without it, so every packet is one send with these flags
	MESSAGE_SEND = MSG_EOR | MSG_NOSIGNAL,
};

struct message_reply {
	// the wire byte, 0 for a reply that is not valid
	char kind;
	// inside the packet, not terminated; empty for a verdict
	const char *text;
	size_t length;
};

// sets the password length; false for a packet that is not one request
bool message_request(const char *data, size_t size, size_t *length);

bool message_reply(const char *data, size_t size, struct message_reply *reply);

#endif
