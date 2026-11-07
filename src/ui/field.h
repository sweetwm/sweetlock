#ifndef SWEETLOCK_UI_FIELD_H
#define SWEETLOCK_UI_FIELD_H

#include <stdbool.h>
#include <stddef.h>

enum field_state {
	FIELD_IDLE,
	FIELD_TYPING,
	FIELD_CHECKING,
	FIELD_FAILED,
};

// the password field's own state: a failure, shown for a moment
struct field {
	// one-shot, ends the failed state
	int timer_fd;
	bool failed;
};

bool field_init(struct field *field);

void field_finish(struct field *field);

void field_fail(struct field *field);

// call when timer_fd is readable
void field_expire(struct field *field);

// derived, so it never disagrees with the password or the attempt
enum field_state field_state(
	const struct field *field, size_t length, bool checking);

#endif
