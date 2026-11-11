#ifndef SWEETLOCK_UI_AVATAR_H
#define SWEETLOCK_UI_AVATAR_H

#include <cairo.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/stat.h>

#include "app/worker.h"
#include "cache/cache.h"

enum {
	// avatar.path, the AccountsService icon, ~/.face
	AVATAR_SOURCES = 3,
	AVATAR_IMAGE_SIZE = 512,
	AVATAR_ERROR_SIZE = 256,
};

enum avatar_phase {
	AVATAR_LOAD,
	AVATAR_SAVE,
};

struct avatar {
	struct worker_job job;
	enum avatar_phase phase;
	// the preview leaves the cache alone
	bool use_cache;
	bool loaded;
	bool cached;
	// decoded with a working cache, so a save can follow
	bool save;
	cairo_surface_t *image;
	struct cache_image pixels;
	struct cache_key key;
	int source;
	char paths[AVATAR_SOURCES][PATH_MAX];
	char errors[AVATAR_SOURCES][AVATAR_ERROR_SIZE];
	char cache_error[AVATAR_ERROR_SIZE];
};

// the cache key of a square cut from file
void avatar_key(const struct stat *file, int32_t size, struct cache_key *key);

// main thread; loads on the worker, right here when worker is NULL, and not
// at all when it failed to start
void avatar_load(struct avatar *avatar, const char *configured,
	const char *login, struct worker *worker);

// main thread, each time the worker hands the job back; true when the UI
// has the avatar to draw
bool avatar_collect(struct avatar *avatar, struct worker *worker);

// after the worker has stopped
void avatar_destroy(struct avatar *avatar);

#endif
