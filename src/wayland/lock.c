#include "wayland/lock.h"

#include <wayland-client.h>

#include "ext-session-lock-v1-client-protocol.h"
#include "log.h"

static double elapsed_ms(const struct timespec *start) {
	struct timespec now = {0};
	clock_gettime(CLOCK_MONOTONIC, &now);
	return (double)(now.tv_sec - start->tv_sec) * 1e3 +
	       (double)(now.tv_nsec - start->tv_nsec) / 1e6;
}

static void send_unlock(struct lock *lock) {
	ext_session_lock_v1_unlock_and_destroy(lock->wl);
	lock->wl = NULL;
	lock->state = LOCK_UNLOCKING;
}

static void lock_locked(void *data, struct ext_session_lock_v1 *wl) {
	(void)wl;
	struct lock *lock = data;
	if (lock->state != LOCK_REQUESTED) {
		return;
	}
	lock->state = LOCK_LOCKED;
	log_info("locked in %.1f ms", elapsed_ms(&lock->start));
	if (lock->unlock_pending) {
		send_unlock(lock);
	}
}

static void lock_finished(void *data, struct ext_session_lock_v1 *wl) {
	(void)wl;
	struct lock *lock = data;
	if (lock->state == LOCK_LOCKED) {
		log_info("the compositor ended the lock");
		send_unlock(lock);
		return;
	}
	log_error("the compositor refused the lock; is the session already "
		  "locked?");
	ext_session_lock_v1_destroy(lock->wl);
	lock->wl = NULL;
	lock->state = LOCK_REFUSED;
}

static const struct ext_session_lock_v1_listener lock_listener = {
	.locked = lock_locked,
	.finished = lock_finished,
};

bool lock_request(struct lock *lock,
	struct ext_session_lock_manager_v1 *manager,
	const struct timespec *start) {
	*lock = (struct lock){.start = *start};
	lock->wl = ext_session_lock_manager_v1_lock(manager);
	if (!lock->wl) {
		log_error("failed to request the session lock");
		return false;
	}
	ext_session_lock_v1_add_listener(lock->wl, &lock_listener, lock);
	lock->state = LOCK_REQUESTED;
	return true;
}

// destroy before locked could race a locked event already in flight, and
// that is a protocol error, so an early unlock waits for locked
void lock_unlock(struct lock *lock) {
	if (lock->state == LOCK_LOCKED) {
		send_unlock(lock);
	} else if (lock->state == LOCK_REQUESTED) {
		lock->unlock_pending = true;
	}
}

void lock_abandon(struct lock *lock) {
	if (lock->wl) {
		wl_proxy_destroy((struct wl_proxy *)lock->wl);
		lock->wl = NULL;
	}
}
