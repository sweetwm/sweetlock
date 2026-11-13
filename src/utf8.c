#include "utf8.h"

#include <stdint.h>
#include <string.h>

static size_t sequence(const unsigned char *text, size_t left) {
	unsigned char first = text[0];
	if (first < 0x80) {
		return first >= 0x20 && first != 0x7f;
	}
	size_t length = 0;
	uint32_t min = 0;
	uint32_t code = 0;
	if ((first & 0xe0) == 0xc0) {
		length = 2;
		min = 0x80;
		code = first & 0x1f;
	} else if ((first & 0xf0) == 0xe0) {
		length = 3;
		min = 0x800;
		code = first & 0x0f;
	} else if ((first & 0xf8) == 0xf0) {
		length = 4;
		min = 0x10000;
		code = first & 0x07;
	} else {
		return 0;
	}
	if (length > left) {
		return 0;
	}
	for (size_t i = 1; i < length; i++) {
		if ((text[i] & 0xc0) != 0x80) {
			return 0;
		}
		code = code << 6 | (text[i] & 0x3f);
	}
	// overlong forms, C1 controls, surrogates and anything past Unicode
	if (code < min || code <= 0x9f || (code >= 0xd800 && code <= 0xdfff) ||
		code > 0x10ffff) {
		return 0;
	}
	// bidi embeddings, overrides and isolates reorder the text around them
	if ((code >= 0x202a && code <= 0x202e) ||
		(code >= 0x2066 && code <= 0x2069)) {
		return 0;
	}
	return length;
}

bool utf8_is_plain(const char *text, size_t length) {
	const unsigned char *bytes = (const unsigned char *)text;
	size_t i = 0;
	while (i < length) {
		size_t step = sequence(bytes + i, length - i);
		if (step == 0) {
			return false;
		}
		i += step;
	}
	return true;
}

size_t utf8_line(const char *text, size_t size, char *out, size_t max) {
	size_t length = 0;
	size_t i = 0;
	while (i < size) {
		size_t step =
			sequence((const unsigned char *)text + i, size - i);
		if (step == 0) {
			// one space per bad character, however many bytes, a
			// leading one is dropped below
			if (length > 0 && length < max) {
				out[length++] = ' ';
			}
			i++;
			while (i < size &&
				((unsigned char)text[i] & 0xc0) == 0x80) {
				i++;
			}
			continue;
		}
		if (length + step > max) {
			break;
		}
		if (length > 0 || text[i] != ' ') {
			memcpy(out + length, text + i, step);
			length += step;
		}
		i += step;
	}
	while (length > 0 && out[length - 1] == ' ') {
		length--;
	}
	return length;
}
