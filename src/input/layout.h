#ifndef SWEETLOCK_INPUT_LAYOUT_H
#define SWEETLOCK_INPUT_LAYOUT_H

#include <limits.h>
#include <stddef.h>

#include "fs/file.h"

enum {
	// far past any xkeyboard-config list
	LAYOUT_LIST_MAX = 1024 * 1024,
	LAYOUT_LABEL_SIZE = 4,
};

enum file_result layout_list_load(char path[PATH_MAX], char **data,
	size_t *length, char *error, size_t error_size);

void layout_label(const char *list, size_t length, const char *name,
	char *label, size_t size);

#endif
