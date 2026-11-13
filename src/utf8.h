#ifndef SWEETLOCK_UTF8_H
#define SWEETLOCK_UTF8_H

#include <stdbool.h>
#include <stddef.h>

bool utf8_is_plain(const char *text, size_t length);

size_t utf8_line(const char *text, size_t size, char *out, size_t max);

#endif
