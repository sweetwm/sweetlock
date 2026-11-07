#include "background/job.h"

#include <cairo.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "background/draw.h"

static void add_color(struct cache_key *key, struct color c) {
	cache_key_add(key, (uint32_t)c.red << 24 | (uint32_t)c.green << 16 |
				   (uint32_t)c.blue << 8 | c.alpha);
}

static void add_int(struct cache_key *key, int32_t value) {
	cache_key_add(key, (uint32_t)value);
}

// a layered background adds its base, then its blobs
static void add_mode(struct cache_key *key, const struct config *config,
	enum background_mode mode) {
	const struct config_gradient *g = &config->background.gradient;
	const struct config_radial *r = &config->background.radial;
	add_int(key, (int32_t)mode);
	switch (mode) {
	case BACKGROUND_SOLID:
		add_color(key, config->background.color);
		break;
	case BACKGROUND_GRADIENT:
		add_color(key, g->top_left);
		add_color(key, g->top_right);
		add_color(key, g->bottom_left);
		add_color(key, g->bottom_right);
		break;
	case BACKGROUND_RADIAL:
		add_color(key, r->center);
		add_color(key, r->edge);
		add_int(key, r->center_x);
		add_int(key, r->center_y);
		add_int(key, r->radius);
		break;
	case BACKGROUND_LAYERED:
		break;
	}
}

void background_key(const struct config *config, int32_t width, int32_t height,
	struct cache_key *key) {
	*key = (struct cache_key){0};
	add_int(key, width);
	add_int(key, height);
	enum background_mode mode = config->background.mode;
	if (mode != BACKGROUND_LAYERED) {
		add_mode(key, config, mode);
		return;
	}
	add_int(key, (int32_t)mode);
	add_mode(key, config, config->background.layered.base);
	add_int(key, config->background.layered.blob_count);
	for (int32_t i = 0; i < config->background.layered.blob_count; i++) {
		const struct config_blob *blob =
			&config->background.layered.blob[i];
		add_color(key, blob->color);
		add_int(key, blob->x);
		add_int(key, blob->y);
		add_int(key, blob->size);
	}
}

static double now_ms(void) {
	struct timespec now = {0};
	clock_gettime(CLOCK_MONOTONIC, &now);
	return (double)now.tv_sec * 1e3 + (double)now.tv_nsec / 1e6;
}

static bool draw(struct background_job *job) {
	const struct cache_image *image = &job->image;
	cairo_surface_t *surface = cairo_image_surface_create_for_data(
		image->pixels, CAIRO_FORMAT_RGB24, image->width, image->height,
		image->stride);
	bool ok = cairo_surface_status(surface) == CAIRO_STATUS_SUCCESS &&
		  background_draw(job->config, surface);
	cairo_surface_destroy(surface);
	if (!ok) {
		snprintf(job->error, sizeof job->error, "failed to draw it");
	}
	return ok;
}

// a cache problem leaves its message and still draws
static void load(struct background_job *job) {
	double start = now_ms();
	int dir = cache_open(job->error, sizeof job->error);
	enum cache_result cached = CACHE_FAILED;
	if (dir >= 0) {
		cached = cache_read(dir, &job->key, &job->image, job->error,
			sizeof job->error);
		close(dir);
	}
	if (cached == CACHE_HIT) {
		job->result = BACKGROUND_CACHED;
	} else if (draw(job)) {
		job->result = BACKGROUND_DRAWN;
		job->save = cached != CACHE_FAILED;
	}
	job->elapsed_ms = now_ms() - start;
}

static void save(struct background_job *job) {
	int dir = cache_open(job->error, sizeof job->error);
	if (dir >= 0) {
		cache_write(dir, &job->key, &job->image, job->error,
			sizeof job->error);
		close(dir);
	}
}

static void run(struct worker_job *base) {
	struct background_job *job = (struct background_job *)base;
	if (job->phase == BACKGROUND_LOAD) {
		load(job);
	} else {
		save(job);
	}
}

void background_job_init(struct background_job *job,
	const struct config *config, const struct cache_image *image) {
	*job = (struct background_job){
		.base = {.run = run},
		.config = config,
		.image = *image,
		.phase = BACKGROUND_LOAD,
		.result = BACKGROUND_FAILED,
	};
	background_key(config, image->width, image->height, &job->key);
}

void background_job_save(struct background_job *job) {
	job->phase = BACKGROUND_SAVE;
	job->error[0] = '\0';
}
