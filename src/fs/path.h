#ifndef SWEETLOCK_FS_PATH_H
#define SWEETLOCK_FS_PATH_H

#include <limits.h>
#include <stdbool.h>

// an over-long result is left empty
bool path_join(char out[PATH_MAX], const char *head, const char *tail);

// HOME, when it is an absolute path
const char *path_home(void);

// a config path (empty, absolute or ~/) as an absolute one; false sets why
// and leaves out empty
bool path_expand(char out[PATH_MAX], const char *configured, const char **why);

#endif
