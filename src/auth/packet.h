#ifndef SWEETLOCK_AUTH_PACKET_H
#define SWEETLOCK_AUTH_PACKET_H

#include <stddef.h>
#include <sys/socket.h>
#include <sys/types.h>

enum {
	// FreeBSD ends a SOCK_SEQPACKET record only at MSG_EOR and merges sends
	// without it, so every packet is one send with these flags
	PACKET_SEND = MSG_EOR | MSG_NOSIGNAL,
};

// recv for one record: a record longer than size returns size + 1, and on
// FreeBSD, which hands out the rest in later reads, the rest is drained
ssize_t packet_receive(int fd, char *data, size_t size, int flags);

#endif
