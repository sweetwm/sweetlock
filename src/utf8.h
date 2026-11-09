#ifndef SWEETLOCK_UTF8_H
#define SWEETLOCK_UTF8_H

#include <stdbool.h>
#include <stddef.h>

// valid UTF-8 without control characters, so it is safe to show as one line
bool utf8_is_plain(const char *text, size_t length);

size_t utf8_plain_sequence(const char *text, size_t left);

#endif
