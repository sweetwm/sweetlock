#include "input/unicode.h"
#include <assert.h>
#include <string.h>

static void test_utf8_sizes(void) {
	assert(utf8_chsize(0x00) == 1);
	assert(utf8_chsize(0x7F) == 1);
	assert(utf8_chsize(0x80) == 2);
	assert(utf8_chsize(0x7FF) == 2);
	assert(utf8_chsize(0x800) == 3);
	assert(utf8_chsize(0xFFFF) == 3);
	assert(utf8_chsize(0x10000) == 4);
}

static void test_utf8_encode(void) {
	char buf[8] = {0};

	assert(utf8_encode(buf, '$') == 1);
	assert(memcmp(buf, "$", 1) == 0);

	memset(buf, 0, sizeof(buf));
	assert(utf8_encode(buf, 0x00A2) == 2);
	assert(memcmp(buf, "\xC2\xA2", 2) == 0);

	memset(buf, 0, sizeof(buf));
	assert(utf8_encode(buf, 0x20AC) == 3);
	assert(memcmp(buf, "\xE2\x82\xAC", 3) == 0);

	memset(buf, 0, sizeof(buf));
	assert(utf8_encode(buf, 0x1F642) == 4);
	assert(memcmp(buf, "\xF0\x9F\x99\x82", 4) == 0);
}

static void test_utf8_last_size(void) {
	assert(utf8_last_size("") == 0);
	assert(utf8_last_size("a") == 1);
	assert(utf8_last_size("a\xC2\xA2") == 2);
	assert(utf8_last_size("a\xE2\x82\xAC") == 3);
	assert(utf8_last_size("a\xF0\x9F\x99\x82") == 4);
}

int main(void) {
	test_utf8_sizes();
	test_utf8_encode();
	test_utf8_last_size();
	return 0;
}
