#include "file.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

// %m, because strerror may share one buffer between threads
static enum file_result fail_errno(char *error, size_t size, int saved) {
	errno = saved;
	snprintf(error, size, "%m");
	return FILE_FAILED;
}

// one byte past size, so a file that grew since fstat is caught
static enum file_result read_all(int fd, size_t size, char **data,
	size_t *length, char *error, size_t error_size) {
	char *buffer = malloc(size + 1);
	if (!buffer) {
		snprintf(error, error_size, "out of memory");
		return FILE_FAILED;
	}
	size_t total = 0;
	while (total <= size) {
		ssize_t n = read(fd, buffer + total, size + 1 - total);
		if (n < 0 && errno == EINTR) {
			continue;
		}
		if (n < 0) {
			free(buffer);
			return fail_errno(error, error_size, errno);
		}
		if (n == 0) {
			break;
		}
		total += (size_t)n;
	}
	if (total > size) {
		free(buffer);
		snprintf(error, error_size, "changed while being read");
		return FILE_FAILED;
	}
	buffer[total] = '\0';
	*data = buffer;
	*length = total;
	return FILE_OK;
}

// O_NONBLOCK so a FIFO at the path cannot hang the lock; it is then refused
enum file_result file_open(const char *path, size_t max, int *fd,
	struct stat *info, char *error, size_t error_size) {
	*fd = open(path, O_RDONLY | O_CLOEXEC | O_NONBLOCK | O_NOCTTY);
	if (*fd < 0) {
		return errno == ENOENT ? FILE_MISSING
				       : fail_errno(error, error_size, errno);
	}
	if (fstat(*fd, info) != 0 || !S_ISREG(info->st_mode)) {
		snprintf(error, error_size, "not a regular file");
	} else if ((uintmax_t)info->st_size > max) {
		snprintf(error, error_size, "larger than %zu bytes", max);
	} else {
		return FILE_OK;
	}
	close(*fd);
	*fd = -1;
	return FILE_FAILED;
}

enum file_result file_read_open(int fd, const struct stat *info, char **data,
	size_t *length, char *error, size_t error_size) {
	*data = NULL;
	*length = 0;
	return read_all(
		fd, (size_t)info->st_size, data, length, error, error_size);
}

enum file_result file_read(const char *path, size_t max, char **data,
	size_t *length, char *error, size_t error_size) {
	*data = NULL;
	*length = 0;
	int fd = -1;
	struct stat info;
	enum file_result result =
		file_open(path, max, &fd, &info, error, error_size);
	if (result == FILE_OK) {
		result = file_read_open(
			fd, &info, data, length, error, error_size);
		close(fd);
	}
	return result;
}
