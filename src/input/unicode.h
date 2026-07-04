#ifndef LOCKY_UNICODE_H
#define LOCKY_UNICODE_H

#include <stddef.h>
#include <stdint.h>

#define UTF8_MAX_SIZE 4

size_t utf8_chsize(uint32_t ch);

size_t utf8_encode(char *str, uint32_t ch);

int utf8_last_size(const char *str);

#endif
