#ifndef SWEETLOCK_WAYLAND_LOCK_H
#define SWEETLOCK_WAYLAND_LOCK_H

#include <stdbool.h>
#include <time.h>

struct ext_session_lock_manager_v1;

enum lock_state {
	LOCK_IDLE,
	LOCK_REQUESTED,
	LOCK_LOCKED,
	LOCK_UNLOCKING,
	LOCK_REFUSED,
};

struct lock {
	struct ext_session_lock_v1 *wl;
	enum lock_state state;
	// an unlock asked for before locked, sent once locked arrives
	bool unlock_pending;
	// when main started, for the timing log
	struct timespec start;
};

bool lock_request(struct lock *lock,
	struct ext_session_lock_manager_v1 *manager,
	const struct timespec *start);

void lock_unlock(struct lock *lock);

// since main started, for the timing log lines
double lock_elapsed_ms(const struct lock *lock);

// forgets the proxy without a request, so the session stays locked
void lock_abandon(struct lock *lock);

#endif
