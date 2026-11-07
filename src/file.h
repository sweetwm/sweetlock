#ifndef SWEETLOCK_FILE_H
#define SWEETLOCK_FILE_H

#include <stddef.h>

enum file_result {
	FILE_OK,
	FILE_MISSING,
	FILE_FAILED,
};

enum file_result file_read(const char *path, size_t max, char **data,
	size_t *length, char *error, size_t error_size);

#endif
