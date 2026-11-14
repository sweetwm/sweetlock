#include "auth/packet.h"

#include <sys/uio.h>
#ifdef __FreeBSD__
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#endif

#ifdef __FreeBSD__
// the tail of a long request is password bytes, so it is wiped
static void drain(int fd, int flags) {
	char rest[64];
	for (bool end = false; !end;) {
		struct iovec part = {.iov_base = rest, .iov_len = sizeof rest};
		struct msghdr message = {.msg_iov = &part, .msg_iovlen = 1};
		ssize_t length = recvmsg(fd, &message, flags);
		if (length < 0 && errno == EINTR) {
			continue;
		}
		end = length <= 0 || (message.msg_flags & MSG_EOR);
	}
	explicit_bzero(rest, sizeof rest);
}
#endif

ssize_t packet_receive(int fd, char *data, size_t size, int flags) {
	struct iovec part = {.iov_base = data, .iov_len = size};
	struct msghdr message = {.msg_iov = &part, .msg_iovlen = 1};
	ssize_t length = recvmsg(fd, &message, flags);
	if (length <= 0) {
		return length;
	}
	if (message.msg_flags & MSG_TRUNC) {
		return (ssize_t)size + 1;
	}
#ifdef __FreeBSD__
	if (!(message.msg_flags & MSG_EOR)) {
		drain(fd, flags);
		return (ssize_t)size + 1;
	}
#endif
	return length;
}
