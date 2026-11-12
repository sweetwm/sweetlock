#include "ui/field.h"

#include <stdint.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include "log.h"

enum { FIELD_FAILED_MS = 1500 };

bool field_init(struct field *field) {
	*field = (struct field){0};
	field->timer_fd =
		timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
	if (field->timer_fd < 0) {
		log_error("failed to create the password field timer: %m");
		return false;
	}
	return true;
}

void field_finish(struct field *field) {
	if (field->timer_fd >= 0) {
		close(field->timer_fd);
	}
	*field = (struct field){.timer_fd = -1};
}

// a failure that could never end would hold back an empty Enter for good
void field_fail(struct field *field) {
	const struct itimerspec spec = {
		.it_value = {.tv_sec = FIELD_FAILED_MS / 1000,
			.tv_nsec = (FIELD_FAILED_MS % 1000) * 1000000L},
	};
	if (timerfd_settime(field->timer_fd, 0, &spec, NULL) != 0) {
		log_warn("failed to time the failed password state: %m");
		return;
	}
	field->failed = true;
}

void field_expire(struct field *field) {
	uint64_t expirations = 0;
	if (read(field->timer_fd, &expirations, sizeof expirations) ==
		(ssize_t)sizeof expirations) {
		field->failed = false;
	}
}

// typing hides a failure, which shows again if the text is erased in time
enum field_state field_state(
	const struct field *field, size_t length, bool checking) {
	if (checking) {
		return FIELD_CHECKING;
	}
	if (length > 0) {
		return FIELD_TYPING;
	}
	return field->failed ? FIELD_FAILED : FIELD_IDLE;
}
