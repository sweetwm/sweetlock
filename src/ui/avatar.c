#include "ui/avatar.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "fs/file.h"
#include "fs/path.h"
#include "image/image.h"
#include "log.h"

static void set_paths(
	struct avatar *avatar, const char *configured, const char *login) {
	const char *why = NULL;
	if (!path_expand(avatar->paths[0], configured, &why)) {
		log_warn("avatar.path: %s; ignoring it", why);
	}
	if (login[0] && !strchr(login, '/')) {
		path_join(avatar->paths[1], "/var/lib/AccountsService/icons/",
			login);
	}
	const char *home = path_home();
	if (home) {
		path_join(avatar->paths[2], home, "/.face");
	}
}

// the centre square, at most AVATAR_IMAGE_SIZE wide
static cairo_surface_t *square(cairo_surface_t *image) {
	int width = cairo_image_surface_get_width(image);
	int height = cairo_image_surface_get_height(image);
	int side = width < height ? width : height;
	int size = side < AVATAR_IMAGE_SIZE ? side : AVATAR_IMAGE_SIZE;
	cairo_surface_t *out =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size, size);
	cairo_t *cr = cairo_create(out);
	cairo_scale(cr, (double)size / side, (double)size / side);
	cairo_set_source_surface(
		cr, image, (side - width) / 2.0, (side - height) / 2.0);
	cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_GOOD);
	cairo_paint(cr);
	cairo_status_t status = cairo_status(cr);
	cairo_destroy(cr);
	if (status != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(out);
		return NULL;
	}
	return out;
}

// a background key has its mode here, which is never this
enum { AVATAR_KEY_TAG = 0x61766174 };

void avatar_key(const struct stat *file, int32_t size, struct cache_key *key) {
	*key = (struct cache_key){0};
	cache_key_add(key, (uint32_t)size);
	cache_key_add(key, (uint32_t)size);
	cache_key_add(key, AVATAR_KEY_TAG);
	cache_key_add_file(key, file);
}

// the side square() will cut, from the header alone; 0 when unknown
static int32_t square_size(const unsigned char *data, size_t length) {
	uint32_t width = 0;
	uint32_t height = 0;
	if (image_probe(data, length, &width, &height) == IMAGE_UNKNOWN ||
		!image_fits(width, height)) {
		return 0;
	}
	uint32_t side = width < height ? width : height;
	return (int32_t)(side < AVATAR_IMAGE_SIZE ? side : AVATAR_IMAGE_SIZE);
}

static cairo_surface_t *read_cached(struct avatar *avatar, int32_t size) {
	int dir = cache_open(avatar->cache_error, AVATAR_ERROR_SIZE);
	if (dir < 0) {
		return NULL;
	}
	cairo_surface_t *image =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size, size);
	enum cache_result result = CACHE_FAILED;
	if (cairo_surface_status(image) == CAIRO_STATUS_SUCCESS) {
		struct cache_image pixels = {
			.pixels = cairo_image_surface_get_data(image),
			.width = size,
			.height = size,
			.stride = cairo_image_surface_get_stride(image),
		};
		result = cache_read(dir, &avatar->key, &pixels,
			avatar->cache_error, AVATAR_ERROR_SIZE);
	}
	close(dir);
	if (result == CACHE_HIT) {
		cairo_surface_mark_dirty(image);
		return image;
	}
	avatar->save = result != CACHE_FAILED;
	cairo_surface_destroy(image);
	return NULL;
}

static bool load_image(struct avatar *avatar, const struct stat *file,
	const unsigned char *data, size_t length, char *error) {
	int32_t size = square_size(data, length);
	avatar->save = false;
	if (avatar->use_cache && size > 0) {
		avatar_key(file, size, &avatar->key);
		avatar->image = read_cached(avatar, size);
		if (avatar->image) {
			avatar->cached = true;
			return true;
		}
	}
	cairo_surface_t *image = image_decode_side(
		data, length, AVATAR_IMAGE_SIZE, error, AVATAR_ERROR_SIZE);
	avatar->image = image ? square(image) : NULL;
	cairo_surface_destroy(image);
	if (!avatar->image) {
		if (image) {
			snprintf(
				error, AVATAR_ERROR_SIZE, "failed to scale it");
		}
		avatar->save = false;
		return false;
	}
	// a square of another size would be read back as a bad entry
	avatar->save = avatar->save &&
		       cairo_image_surface_get_width(avatar->image) == size;
	return true;
}

// the file is opened once, so its key and its pixels come from one file
static enum file_result load_source(struct avatar *avatar, int i) {
	char *error = avatar->errors[i];
	int fd = -1;
	struct stat file = {0};
	enum file_result result = file_open(avatar->paths[i], IMAGE_FILE_MAX,
		&fd, &file, error, AVATAR_ERROR_SIZE);
	if (result != FILE_OK) {
		return result;
	}
	char *data = NULL;
	size_t length = 0;
	result = file_read_open(
		fd, &file, &data, &length, error, AVATAR_ERROR_SIZE);
	close(fd);
	if (result == FILE_OK &&
		!load_image(avatar, &file, (const unsigned char *)data, length,
			error)) {
		result = FILE_FAILED;
	}
	free(data);
	return result;
}

static void save(struct avatar *avatar) {
	int dir = cache_open(avatar->cache_error, AVATAR_ERROR_SIZE);
	if (dir >= 0) {
		cache_write(dir, &avatar->key, &avatar->pixels,
			avatar->cache_error, AVATAR_ERROR_SIZE);
		close(dir);
	}
}

// the worker's side: the first source that loads wins
static void run(struct worker_job *job) {
	struct avatar *avatar = (struct avatar *)job;
	if (avatar->phase == AVATAR_SAVE) {
		save(avatar);
		return;
	}
	for (int i = 0; i < AVATAR_SOURCES; i++) {
		enum file_result result = FILE_MISSING;
		if (avatar->paths[i][0]) {
			result = load_source(avatar, i);
		}
		if (result == FILE_OK) {
			avatar->source = i;
			return;
		}
		// only the configured source is expected to exist
		if (result == FILE_MISSING && i == 0 && avatar->paths[i][0]) {
			snprintf(avatar->errors[i], AVATAR_ERROR_SIZE,
				"no such file");
		}
	}
}

static void finish(struct avatar *avatar) {
	for (int i = 0; i < AVATAR_SOURCES; i++) {
		if (avatar->errors[i][0]) {
			log_warn("avatar %s: %s", avatar->paths[i],
				avatar->errors[i]);
		}
	}
	if (avatar->cache_error[0]) {
		log_warn("avatar cache: %s", avatar->cache_error);
	}
	if (avatar->image) {
		log_info("avatar from %s%s", avatar->paths[avatar->source],
			avatar->cached ? " (read from the cache)" : "");
	}
	avatar->loaded = true;
}

void avatar_load(struct avatar *avatar, const char *configured,
	const char *login, struct worker *worker) {
	*avatar = (struct avatar){
		.job = {.run = run},
		.use_cache = worker != NULL,
	};
	set_paths(avatar, configured, login);
	if (!worker) {
		run(&avatar->job);
	} else if (worker->fd >= 0) {
		worker_submit(worker, &avatar->job);
		return;
	}
	finish(avatar);
}

bool avatar_collect(struct avatar *avatar, struct worker *worker) {
	if (avatar->phase == AVATAR_SAVE) {
		if (avatar->cache_error[0]) {
			log_warn("avatar cache: %s", avatar->cache_error);
		} else {
			log_debug("avatar saved to the cache");
		}
		return false;
	}
	finish(avatar);
	// the pixels are final, and only read from now on
	if (avatar->save) {
		cairo_surface_flush(avatar->image);
		avatar->pixels = (struct cache_image){
			.pixels = cairo_image_surface_get_data(avatar->image),
			.width = cairo_image_surface_get_width(avatar->image),
			.height = cairo_image_surface_get_height(avatar->image),
			.stride = cairo_image_surface_get_stride(avatar->image),
		};
		avatar->phase = AVATAR_SAVE;
		avatar->cache_error[0] = '\0';
		worker_submit(worker, &avatar->job);
	}
	return true;
}

void avatar_destroy(struct avatar *avatar) {
	cairo_surface_destroy(avatar->image);
	avatar->image = NULL;
}
