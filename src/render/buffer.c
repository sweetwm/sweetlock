#include "render/buffer.h"
#include "app/log.h"
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

static void buffer_handle_release(void *data, struct wl_buffer *wl_buffer) {
	(void)wl_buffer;
	struct locky_buffer *buffer = data;
	buffer->busy = false;
}

static const struct wl_buffer_listener buffer_listener = {
	.release = buffer_handle_release,
};

static void buffer_finish(struct locky_buffer *buffer) {
	if (buffer->cairo != NULL) {
		cairo_destroy(buffer->cairo);
	}
	if (buffer->cairo_surface != NULL) {
		cairo_surface_destroy(buffer->cairo_surface);
	}
	if (buffer->wl_buffer != NULL) {
		wl_buffer_destroy(buffer->wl_buffer);
	}
	if (buffer->data != NULL) {
		munmap(buffer->data, buffer->size);
	}
	memset(buffer, 0, sizeof(*buffer));
}

static bool buffer_init(struct locky_buffer *buffer, struct wl_shm *shm,
	int32_t width, int32_t height) {
	uint32_t stride = (uint32_t)width * 4;
	size_t size = (size_t)stride * (size_t)height;

	int fd = memfd_create("locky-buffer", MFD_CLOEXEC);
	if (fd < 0) {
		locky_log(LOCKY_ERROR, "memfd_create failed");
		return false;
	}
	if (ftruncate(fd, (off_t)size) < 0) {
		locky_log(LOCKY_ERROR, "ftruncate failed");
		close(fd);
		return false;
	}

	void *data =
		mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (data == MAP_FAILED) {
		locky_log(LOCKY_ERROR, "mmap failed");
		close(fd);
		return false;
	}

	struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, (int32_t)size);
	buffer->wl_buffer = wl_shm_pool_create_buffer(pool, 0, width, height,
		(int32_t)stride, WL_SHM_FORMAT_ARGB8888);
	wl_shm_pool_destroy(pool);
	close(fd);

	if (buffer->wl_buffer == NULL) {
		munmap(data, size);
		return false;
	}

	buffer->cairo_surface = cairo_image_surface_create_for_data(
		data, CAIRO_FORMAT_ARGB32, width, height, (int)stride);
	if (cairo_surface_status(buffer->cairo_surface) !=
		CAIRO_STATUS_SUCCESS) {
		locky_log(LOCKY_ERROR, "failed to create cairo surface");
		cairo_surface_destroy(buffer->cairo_surface);
		wl_buffer_destroy(buffer->wl_buffer);
		munmap(data, size);
		memset(buffer, 0, sizeof(*buffer));
		return false;
	}
	buffer->cairo = cairo_create(buffer->cairo_surface);

	buffer->data = data;
	buffer->size = size;
	buffer->width = width;
	buffer->height = height;

	wl_buffer_add_listener(buffer->wl_buffer, &buffer_listener, buffer);
	return true;
}

struct locky_buffer *locky_pool_get_buffer(struct locky_buffer pool[static 2],
	struct wl_shm *shm, int32_t width, int32_t height) {
	if (width <= 0 || height <= 0) {
		return NULL;
	}

	struct locky_buffer *buffer = NULL;
	for (size_t i = 0; i < 2; i++) {
		if (!pool[i].busy) {
			buffer = &pool[i];
			break;
		}
	}
	if (buffer == NULL) {
		locky_log(LOCKY_ERROR, "no free buffer in pool");
		return NULL;
	}

	if (buffer->wl_buffer != NULL &&
		(buffer->width != width || buffer->height != height)) {
		buffer_finish(buffer);
	}
	if (buffer->wl_buffer == NULL &&
		!buffer_init(buffer, shm, width, height)) {
		return NULL;
	}

	buffer->busy = true;
	return buffer;
}

void locky_pool_destroy(struct locky_buffer pool[static 2]) {
	for (size_t i = 0; i < 2; i++) {
		buffer_finish(&pool[i]);
	}
}
