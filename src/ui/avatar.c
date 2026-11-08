#include "ui/avatar.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "image.h"
#include "log.h"
#include "path.h"

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

// the worker's side: the first source that loads wins
static void run(struct worker_job *job) {
	struct avatar *avatar = (struct avatar *)job;
	for (int i = 0; i < AVATAR_SOURCES; i++) {
		char *error = avatar->errors[i];
		cairo_surface_t *image = NULL;
		enum file_result result = FILE_MISSING;
		if (avatar->paths[i][0]) {
			result = image_load(avatar->paths[i], AVATAR_IMAGE_SIZE,
				&image, error, AVATAR_ERROR_SIZE);
		}
		if (result == FILE_OK) {
			avatar->image = square(image);
			cairo_surface_destroy(image);
			if (avatar->image) {
				avatar->source = i;
				return;
			}
			snprintf(
				error, AVATAR_ERROR_SIZE, "failed to scale it");
		} else if (result == FILE_MISSING && i == 0 &&
			   avatar->paths[i][0]) {
			// only the configured source is expected to exist
			snprintf(error, AVATAR_ERROR_SIZE, "no such file");
		}
	}
}

void avatar_load(struct avatar *avatar, const char *configured,
	const char *login, struct worker *worker) {
	*avatar = (struct avatar){.job = {.run = run}};
	set_paths(avatar, configured, login);
	if (!worker) {
		run(&avatar->job);
	} else if (worker->started) {
		worker_submit(worker, &avatar->job);
		return;
	}
	avatar_finish(avatar);
}

void avatar_finish(struct avatar *avatar) {
	for (int i = 0; i < AVATAR_SOURCES; i++) {
		if (avatar->errors[i][0]) {
			log_warn("avatar %s: %s", avatar->paths[i],
				avatar->errors[i]);
		}
	}
	if (avatar->image) {
		log_info("avatar from %s", avatar->paths[avatar->source]);
	}
	avatar->state = avatar->image ? AVATAR_IMAGE : AVATAR_FALLBACK;
}

void avatar_destroy(struct avatar *avatar) {
	cairo_surface_destroy(avatar->image);
	avatar->image = NULL;
}
