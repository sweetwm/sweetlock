#ifndef SWEETLOCK_WAYLAND_BACKDROP_H
#define SWEETLOCK_WAYLAND_BACKDROP_H

#include <stdbool.h>
#include <stdint.h>
#include <wayland-util.h>

#include "background/job.h"

struct config;
struct display;
struct worker;

struct backdrop {
	struct wl_list link;
	int32_t width;
	int32_t height;
	// background_scale's, so outputs share unless the scale changes pixels
	uint32_t scale;
	struct buffer *buffer;
	struct background_job job;
	// the pixels are final and can be attached; never set after a failure
	bool ready;
	// with the worker, so neither the job nor the pixels may be touched
	bool working;
	int refs;
};

struct backdrop *backdrop_get(struct display *display,
	const struct config *config, struct worker *worker, int32_t width,
	int32_t height, uint32_t scale);

// a backdrop without references goes once the worker has handed it back
void backdrop_put(struct backdrop *backdrop);

// main thread, for each job the worker hands back; false when the job is
// no backdrop's
bool backdrop_collect(
	struct display *display, struct worker *worker, struct worker_job *job);

// only once the worker has stopped
void backdrop_destroy(struct backdrop *backdrop);

#endif
