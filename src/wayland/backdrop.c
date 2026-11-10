#include "wayland/backdrop.h"

#include <stdlib.h>

#include "app/worker.h"
#include "log.h"
#include "wayland/buffer.h"
#include "wayland/display.h"

struct backdrop *backdrop_get(struct display *display,
	const struct config *config, struct worker *worker, int32_t width,
	int32_t height, uint32_t scale) {
	uint32_t shared = background_scale(config, scale);
	struct backdrop *backdrop = NULL;
	wl_list_for_each(backdrop, &display->backdrops, link) {
		if (backdrop->width == width && backdrop->height == height &&
			backdrop->scale == shared) {
			backdrop->refs++;
			return backdrop;
		}
	}
	backdrop = calloc(1, sizeof *backdrop);
	if (!backdrop) {
		log_error("out of memory allocating a background");
		return NULL;
	}
	*backdrop = (struct backdrop){
		.width = width,
		.height = height,
		.scale = shared,
		.refs = 1,
	};
	wl_list_insert(&display->backdrops, &backdrop->link);
	// a failure keeps the base colour, and is remembered so it logs once
	backdrop->buffer = buffer_create(display->shm, width, height, true);
	if (!backdrop->buffer) {
		return backdrop;
	}
	if (!worker || !worker->started) {
		log_warn("no render worker; the background stays the base "
			 "colour");
		return backdrop;
	}
	struct cache_image image = {
		.pixels = backdrop->buffer->data,
		.width = width,
		.height = height,
		.stride = width * 4,
	};
	background_job_init(&backdrop->job, config, &image, scale);
	backdrop->working = true;
	worker_submit(worker, &backdrop->job.base);
	return backdrop;
}

void backdrop_put(struct backdrop *backdrop) {
	if (backdrop && --backdrop->refs == 0 && !backdrop->working) {
		backdrop_destroy(backdrop);
	}
}

static void loaded(struct display *display, struct worker *worker,
	struct backdrop *backdrop) {
	struct background_job *job = &backdrop->job;
	if (job->result == BACKGROUND_FAILED) {
		// every output fails the same way, so once is enough
		if (!display->background_warned && job->path[0]) {
			log_warn("background image %s: %s; keeping the base "
				 "colour",
				job->path, job->error);
		} else if (!display->background_warned) {
			log_warn("background %dx%d: %s; keeping the base "
				 "colour",
				backdrop->width, backdrop->height, job->error);
		}
		display->background_warned = true;
		return;
	}
	if (job->error[0]) {
		log_warn("background cache: %s", job->error);
	}
	log_info("background %dx%d ready in %.1f ms (%s in %.1f ms)",
		backdrop->width, backdrop->height,
		lock_elapsed_ms(&display->lock),
		job->result == BACKGROUND_CACHED ? "read from the cache"
						 : "drawn",
		job->elapsed_ms);
	backdrop->ready = true;
	// the pixels are final, and only read from now on
	if (job->save) {
		background_job_save(job);
		backdrop->working = true;
		worker_submit(worker, &job->base);
	}
}

bool backdrop_collect(struct display *display, struct worker *worker,
	struct worker_job *job) {
	struct backdrop *backdrop = NULL;
	wl_list_for_each(backdrop, &display->backdrops, link) {
		if (&backdrop->job.base == job) {
			break;
		}
	}
	if (&backdrop->link == &display->backdrops) {
		return false;
	}
	backdrop->working = false;
	if (backdrop->refs == 0) {
		backdrop_destroy(backdrop);
	} else if (backdrop->job.phase == BACKGROUND_LOAD) {
		loaded(display, worker, backdrop);
	} else if (backdrop->job.error[0]) {
		log_warn("background cache: %s", backdrop->job.error);
	} else {
		log_debug("background %dx%d saved to the cache",
			backdrop->width, backdrop->height);
	}
	return true;
}

void backdrop_destroy(struct backdrop *backdrop) {
	wl_list_remove(&backdrop->link);
	buffer_destroy(backdrop->buffer);
	free(backdrop);
}
