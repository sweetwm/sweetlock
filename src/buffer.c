#include "locky/buffer.h"
#include "locky/log.h"
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

struct locky_buffer {
	struct wl_buffer *wl_buffer;
	void *data;
	size_t size;
};

static void buffer_handle_release(void *data, struct wl_buffer *wl_buffer) {
	(void)wl_buffer;
	struct locky_buffer *buffer = data;
	wl_buffer_destroy(buffer->wl_buffer);
	munmap(buffer->data, buffer->size);
	free(buffer);
}

static const struct wl_buffer_listener buffer_listener = {
	.release = buffer_handle_release,
};

struct wl_buffer *locky_buffer_create_solid(
	struct wl_shm *shm, int32_t width, int32_t height, uint32_t color) {
	if (width <= 0 || height <= 0) {
		return NULL;
	}

	int32_t stride = width * 4;
	size_t size = (size_t)stride * (size_t)height;

	int fd = memfd_create("locky-buffer", MFD_CLOEXEC);
	if (fd < 0) {
		locky_log(LOCKY_ERROR, "memfd_create failed");
		return NULL;
	}
	if (ftruncate(fd, (off_t)size) < 0) {
		locky_log(LOCKY_ERROR, "ftruncate failed");
		close(fd);
		return NULL;
	}

	void *data =
		mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (data == MAP_FAILED) {
		locky_log(LOCKY_ERROR, "mmap failed");
		close(fd);
		return NULL;
	}

	struct locky_buffer *buffer = calloc(1, sizeof(*buffer));
	if (buffer == NULL) {
		munmap(data, size);
		close(fd);
		return NULL;
	}

	struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, (int32_t)size);
	buffer->wl_buffer = wl_shm_pool_create_buffer(
		pool, 0, width, height, stride, WL_SHM_FORMAT_XRGB8888);
	wl_shm_pool_destroy(pool);
	close(fd);

	if (buffer->wl_buffer == NULL) {
		munmap(data, size);
		free(buffer);
		return NULL;
	}

	buffer->data = data;
	buffer->size = size;

	uint32_t *pixels = data;
	for (size_t i = 0; i < size / 4; i++) {
		pixels[i] = color;
	}

	wl_buffer_add_listener(buffer->wl_buffer, &buffer_listener, buffer);
	return buffer->wl_buffer;
}
