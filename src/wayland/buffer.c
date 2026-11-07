#include "wayland/buffer.h"

#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>

#include "log.h"

struct wl_buffer *buffer_solid(struct wl_shm *shm, struct color color) {
	// XRGB8888 is a little-endian word, so the bytes run blue first
	const uint8_t pixel[4] = {color.blue, color.green, color.red, 0xff};
	int fd = memfd_create("sweetlock-solid", MFD_CLOEXEC);
	if (fd < 0 || write(fd, pixel, sizeof pixel) != (ssize_t)sizeof pixel) {
		log_error("failed to create the solid colour buffer: %m");
		if (fd >= 0) {
			close(fd);
		}
		return NULL;
	}

	struct wl_buffer *buffer = NULL;
	struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, sizeof pixel);
	if (pool) {
		buffer = wl_shm_pool_create_buffer(
			pool, 0, 1, 1, sizeof pixel, WL_SHM_FORMAT_XRGB8888);
		wl_shm_pool_destroy(pool);
	}
	close(fd);
	if (!buffer) {
		log_error("failed to create the solid colour wl_buffer");
	}
	return buffer;
}

enum { BYTES_PER_PIXEL = 4 };

static void buffer_release(void *data, struct wl_buffer *wl) {
	(void)wl;
	struct buffer *buffer = data;
	buffer->busy = false;
}

static const struct wl_buffer_listener buffer_listener = {
	.release = buffer_release,
};

// a wl_shm_pool size is an int32, which bounds the whole buffer
static bool buffer_layout(int32_t width, int32_t height, int32_t *stride) {
	if (width <= 0 || height <= 0 || width > INT32_MAX / BYTES_PER_PIXEL) {
		return false;
	}
	*stride = width * BYTES_PER_PIXEL;
	return height <= INT32_MAX / *stride;
}

// the fd stays open for the pool, -1 on failure
static int map(struct buffer *buffer) {
	int fd = memfd_create("sweetlock-buffer", MFD_CLOEXEC);
	if (fd < 0 || ftruncate(fd, (off_t)buffer->size) != 0) {
		log_error("failed to create buffer memory: %m");
	} else {
		buffer->data = mmap(NULL, buffer->size, PROT_READ | PROT_WRITE,
			MAP_SHARED, fd, 0);
		if (buffer->data != MAP_FAILED) {
			return fd;
		}
		log_error("failed to map buffer memory: %m");
	}
	if (fd >= 0) {
		close(fd);
	}
	return -1;
}

static bool create_wl(struct buffer *buffer, struct wl_shm *shm, int fd,
	int32_t stride, bool opaque) {
	struct wl_shm_pool *pool =
		wl_shm_create_pool(shm, fd, (int32_t)buffer->size);
	if (pool) {
		buffer->wl = wl_shm_pool_create_buffer(pool, 0, buffer->width,
			buffer->height, stride,
			opaque ? WL_SHM_FORMAT_XRGB8888
			       : WL_SHM_FORMAT_ARGB8888);
		wl_shm_pool_destroy(pool);
	}
	if (!buffer->wl) {
		log_error("failed to create a wl_buffer");
		return false;
	}
	wl_buffer_add_listener(buffer->wl, &buffer_listener, buffer);
	return true;
}

struct buffer *buffer_create(
	struct wl_shm *shm, int32_t width, int32_t height, bool opaque) {
	int32_t stride = 0;
	if (!buffer_layout(width, height, &stride)) {
		log_error("buffer size %dx%d is out of range", width, height);
		return NULL;
	}
	struct buffer *buffer = calloc(1, sizeof *buffer);
	if (!buffer) {
		log_error("out of memory allocating a buffer");
		return NULL;
	}
	*buffer = (struct buffer){
		.data = MAP_FAILED,
		.size = (size_t)stride * (size_t)height,
		.width = width,
		.height = height,
	};

	int fd = map(buffer);
	bool ok = fd >= 0 && create_wl(buffer, shm, fd, stride, opaque);
	if (fd >= 0) {
		close(fd);
	}
	if (!ok) {
		buffer_destroy(buffer);
		return NULL;
	}
	// the same memory layouts as the two wl_shm formats
	cairo_format_t format =
		opaque ? CAIRO_FORMAT_RGB24 : CAIRO_FORMAT_ARGB32;
	buffer->cairo = cairo_image_surface_create_for_data(
		buffer->data, format, width, height, stride);
	if (cairo_surface_status(buffer->cairo) != CAIRO_STATUS_SUCCESS ||
		cairo_format_stride_for_width(format, width) != stride) {
		log_error("failed to create a Cairo surface for the buffer");
		buffer_destroy(buffer);
		return NULL;
	}
	return buffer;
}

void buffer_destroy(struct buffer *buffer) {
	if (!buffer) {
		return;
	}
	if (buffer->cairo) {
		cairo_surface_destroy(buffer->cairo);
	}
	if (buffer->wl) {
		wl_buffer_destroy(buffer->wl);
	}
	if (buffer->data != MAP_FAILED) {
		munmap(buffer->data, buffer->size);
	}
	free(buffer);
}
