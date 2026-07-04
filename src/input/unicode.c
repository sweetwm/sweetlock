#include "input/unicode.h"
#include <string.h>

size_t utf8_chsize(uint32_t ch) {
	if (ch < 0x80) {
		return 1;
	} else if (ch < 0x800) {
		return 2;
	} else if (ch < 0x10000) {
		return 3;
	}
	return 4;
}

size_t utf8_encode(char *str, uint32_t ch) {
	size_t len;
	uint8_t first;

	if (ch < 0x80) {
		first = 0;
		len = 1;
	} else if (ch < 0x800) {
		first = 0xc0;
		len = 2;
	} else if (ch < 0x10000) {
		first = 0xe0;
		len = 3;
	} else {
		first = 0xf0;
		len = 4;
	}

	for (size_t i = len - 1; i > 0; --i) {
		str[i] = (char)((ch & 0x3f) | 0x80);
		ch >>= 6;
	}
	str[0] = (char)(ch | first);
	return len;
}

int utf8_last_size(const char *str) {
	int len = 0;
	const char *pos = strchr(str, '\0');
	while (pos > str) {
		--pos;
		++len;
		// stop at the leading byte of the last character
		if ((*pos & 0xc0) != 0x80) {
			return len;
		}
	}
	return 0;
}
