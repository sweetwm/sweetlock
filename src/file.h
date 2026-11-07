#ifndef SWEETLOCK_FILE_H
#define SWEETLOCK_FILE_H

#include <stddef.h>
#include <sys/stat.h>

enum file_result {
	FILE_OK,
	FILE_MISSING,
	FILE_FAILED,
};

enum file_result file_open(const char *path, size_t max, int *fd,
	struct stat *info, char *error, size_t error_size);

// all of a file from file_open, checked against the size it had then
enum file_result file_read_open(int fd, const struct stat *info, char **data,
	size_t *length, char *error, size_t error_size);

enum file_result file_read(const char *path, size_t max, char **data,
	size_t *length, char *error, size_t error_size);

#endif
