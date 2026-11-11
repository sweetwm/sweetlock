#ifndef SWEETLOCK_BACKGROUND_JOB_H
#define SWEETLOCK_BACKGROUND_JOB_H

#include <cairo.h>
#include <limits.h>
#include <stdbool.h>
#include <sys/stat.h>

#include "app/worker.h"
#include "cache/cache.h"
#include "config/config.h"

enum { BACKGROUND_ERROR_SIZE = 256 };

enum background_phase {
	BACKGROUND_LOAD,
	BACKGROUND_SAVE,
};

enum background_result {
	BACKGROUND_FAILED,
	BACKGROUND_CACHED,
	BACKGROUND_DRAWN,
};

// fills image with a generated background on the worker
struct background_job {
	struct worker_job base;
	const struct config *config;
	// background.image.path expanded; empty after a failure
	char path[PATH_MAX];
	uint32_t scale;
	struct cache_image image;
	struct cache_key key;
	enum background_phase phase;
	enum background_result result;
	// the load missed with a working cache, so a save can follow
	bool save;
	// worker time for the load
	double elapsed_ms;
	// for the main thread to log
	char error[BACKGROUND_ERROR_SIZE];
};

// the scale when it changes the pixels, otherwise 0
uint32_t background_scale(const struct config *config, uint32_t scale);

void background_key(const struct config *config, int32_t width, int32_t height,
	uint32_t scale, const struct stat *file, struct cache_key *key);

// main thread; config and the pixels must outlive the job
void background_job_init(struct background_job *job,
	const struct config *config, const struct cache_image *image,
	uint32_t scale);

// draws without the cache, on the calling thread, as the preview does
bool background_paint(const struct config *config, uint32_t scale,
	cairo_surface_t *target, char *error, size_t error_size);

void background_job_save(struct background_job *job);

#endif
