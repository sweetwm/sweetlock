#include "background/job.h"

#include <cairo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "background/draw.h"
#include "background/wallpaper.h"
#include "file.h"
#include "image.h"
#include "path.h"

static void add_color(struct cache_key *key, struct color c) {
	cache_key_add(key, (uint32_t)c.red << 24 | (uint32_t)c.green << 16 |
				   (uint32_t)c.blue << 8 | c.alpha);
}

static void add_int(struct cache_key *key, int32_t value) {
	cache_key_add(key, (uint32_t)value);
}

static void add_u64(struct cache_key *key, uint64_t value) {
	cache_key_add(key, (uint32_t)value);
	cache_key_add(key, (uint32_t)(value >> 32));
}

// FNV-1a; the inode already tells files apart
static uint64_t hash(const char *text) {
	uint64_t h = 0xcbf29ce484222325u;
	for (; *text; text++) {
		h = (h ^ (unsigned char)*text) * 0x100000001b3u;
	}
	return h;
}

static void add_image(struct cache_key *key, const struct config *config,
	uint32_t scale, const struct stat *file) {
	add_color(key, config->background.color);
	add_u64(key, hash(config->background.image.path));
	add_u64(key, file ? (uint64_t)file->st_dev : 0);
	add_u64(key, file ? (uint64_t)file->st_ino : 0);
	add_u64(key, file ? (uint64_t)file->st_size : 0);
	add_u64(key, file ? (uint64_t)file->st_mtim.tv_sec : 0);
	add_int(key, file ? (int32_t)file->st_mtim.tv_nsec : 0);
	add_int(key, (int32_t)config->background.image.fit);
	add_int(key, config->background.image.dim);
	add_int(key, config->background.image.blur);
	cache_key_add(key, background_scale(config, scale));
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
	case BACKGROUND_IMAGE:
		break;
	}
}

// blur is in logical pixels; nothing else depends on the scale
uint32_t background_scale(const struct config *config, uint32_t scale) {
	return config->background.mode == BACKGROUND_IMAGE &&
			       config->background.image.blur > 0
		       ? scale
		       : 0;
}

void background_key(const struct config *config, int32_t width, int32_t height,
	uint32_t scale, const struct stat *file, struct cache_key *key) {
	*key = (struct cache_key){0};
	add_int(key, width);
	add_int(key, height);
	enum background_mode mode = config->background.mode;
	if (mode == BACKGROUND_IMAGE) {
		add_int(key, (int32_t)mode);
		add_image(key, config, scale, file);
		return;
	}
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

static bool draw_image(struct background_job *job, int fd,
	const struct stat *file, cairo_surface_t *surface) {
	char *data = NULL;
	size_t length = 0;
	bool ok =
		file_read_open(fd, file, &data, &length, job->error,
			sizeof job->error) == FILE_OK &&
		wallpaper_draw((const unsigned char *)data, length, job->config,
			job->scale, surface, job->error, sizeof job->error);
	free(data);
	return ok;
}

// fd is the open image, or -1 for a generated background
static bool draw(struct background_job *job, int fd, const struct stat *file) {
	const struct cache_image *image = &job->image;
	cairo_surface_t *surface = cairo_image_surface_create_for_data(
		image->pixels, CAIRO_FORMAT_RGB24, image->width, image->height,
		image->stride);
	bool ok = cairo_surface_status(surface) == CAIRO_STATUS_SUCCESS;
	if (ok && fd >= 0) {
		ok = draw_image(job, fd, file, surface);
	} else if (ok) {
		ok = background_draw(job->config, surface);
	}
	cairo_surface_destroy(surface);
	if (!ok && !job->error[0]) {
		snprintf(job->error, sizeof job->error, "failed to draw it");
	}
	return ok;
}

// the image is opened once, so its key and its pixels come from one file
static bool open_image(struct background_job *job, int *fd, struct stat *file) {
	if (!job->path[0]) {
		return false;
	}
	enum file_result result = file_open(job->path, IMAGE_FILE_MAX, fd, file,
		job->error, sizeof job->error);
	if (result == FILE_MISSING) {
		snprintf(job->error, sizeof job->error, "no such file");
	}
	return result == FILE_OK;
}

// a cache problem leaves its message and still draws
static void load(struct background_job *job) {
	double start = now_ms();
	int fd = -1;
	struct stat file = {0};
	if (job->config->background.mode == BACKGROUND_IMAGE &&
		!open_image(job, &fd, &file)) {
		job->elapsed_ms = now_ms() - start;
		return;
	}
	background_key(job->config, job->image.width, job->image.height,
		job->scale, fd >= 0 ? &file : NULL, &job->key);
	int dir = cache_open(job->error, sizeof job->error);
	enum cache_result cached = CACHE_FAILED;
	if (dir >= 0) {
		cached = cache_read(dir, &job->key, &job->image, job->error,
			sizeof job->error);
		close(dir);
	}
	if (cached == CACHE_HIT) {
		job->result = BACKGROUND_CACHED;
	} else if (draw(job, fd, &file)) {
		job->result = BACKGROUND_DRAWN;
		job->save = cached != CACHE_FAILED;
	}
	if (fd >= 0) {
		close(fd);
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

// main thread, since it reads HOME
static bool image_path(const struct config *config, char path[PATH_MAX],
	char *error, size_t error_size) {
	const char *why = NULL;
	if (!path_expand(path, config->background.image.path, &why)) {
		snprintf(error, error_size, "%s", why);
		return false;
	}
	if (!path[0]) {
		snprintf(error, error_size, "background.image.path is empty");
		return false;
	}
	return true;
}

void background_job_init(struct background_job *job,
	const struct config *config, const struct cache_image *image,
	uint32_t scale) {
	*job = (struct background_job){
		.base = {.run = run},
		.config = config,
		.scale = scale,
		.image = *image,
		.phase = BACKGROUND_LOAD,
		.result = BACKGROUND_FAILED,
	};
	if (config->background.mode == BACKGROUND_IMAGE) {
		image_path(config, job->path, job->error, sizeof job->error);
	}
}

void background_job_save(struct background_job *job) {
	job->phase = BACKGROUND_SAVE;
	job->error[0] = '\0';
}

bool background_paint(const struct config *config, uint32_t scale,
	cairo_surface_t *target, char *error, size_t error_size) {
	if (config->background.mode != BACKGROUND_IMAGE) {
		bool ok = background_draw(config, target);
		if (!ok) {
			snprintf(error, error_size, "failed to draw it");
		}
		return ok;
	}
	char path[PATH_MAX];
	char *data = NULL;
	size_t length = 0;
	if (!image_path(config, path, error, error_size)) {
		return false;
	}
	enum file_result result = file_read(
		path, IMAGE_FILE_MAX, &data, &length, error, error_size);
	if (result == FILE_MISSING) {
		snprintf(error, error_size, "no such file");
	}
	bool ok = result == FILE_OK &&
		  wallpaper_draw((const unsigned char *)data, length, config,
			  scale, target, error, error_size);
	free(data);
	return ok;
}
