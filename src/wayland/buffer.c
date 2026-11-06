#include "wayland/buffer.h"

#include <stdint.h>
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
