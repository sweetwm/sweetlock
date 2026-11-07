#ifndef SWEETLOCK_UI_AVATAR_H
#define SWEETLOCK_UI_AVATAR_H

#include <cairo.h>
#include <limits.h>

#include "worker.h"

enum {
	// avatar.path, the AccountsService icon, ~/.face
	AVATAR_SOURCES = 3,
	AVATAR_IMAGE_SIZE = 512,
	AVATAR_ERROR_SIZE = 256,
};

enum avatar_state {
	AVATAR_LOADING,
	AVATAR_IMAGE,
	AVATAR_FALLBACK,
};

struct avatar {
	struct worker_job job;
	enum avatar_state state;
	cairo_surface_t *image;
	int source;
	char paths[AVATAR_SOURCES][PATH_MAX];
	char errors[AVATAR_SOURCES][AVATAR_ERROR_SIZE];
};

// main thread; loads on the worker, right here when worker is NULL, and not
// at all when it failed to start
void avatar_load(struct avatar *avatar, const char *configured,
	const char *login, struct worker *worker);

// main thread, once the worker hands the job back; logs what failed
void avatar_finish(struct avatar *avatar);

// after the worker has stopped
void avatar_destroy(struct avatar *avatar);

#endif
