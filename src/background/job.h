#ifndef SWEETLOCK_BACKGROUND_JOB_H
#define SWEETLOCK_BACKGROUND_JOB_H

#include <stdbool.h>

#include "background/cache.h"
#include "config.h"
#include "worker.h"

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

// everything in the config and the image size that changes the pixels
void background_key(const struct config *config, int32_t width, int32_t height,
	struct cache_key *key);

// main thread; config and the pixels must outlive the job
void background_job_init(struct background_job *job,
	const struct config *config, const struct cache_image *image);

void background_job_save(struct background_job *job);

#endif
