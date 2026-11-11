#ifndef SWEETLOCK_BACKGROUND_CACHE_H
#define SWEETLOCK_BACKGROUND_CACHE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
	CACHE_KEY_MAX = 256,
	CACHE_HEADER_SIZE = 8 + 5 * 4,
	CACHE_ENTRIES_MAX = 6,
	CACHE_VERSION = 1,
};

// every input that changes the pixels, in a fixed byte layout
struct cache_key {
	unsigned char bytes[CACHE_KEY_MAX];
	size_t length;
};

// 4 bytes a pixel, rows stride bytes apart
struct cache_image {
	void *pixels;
	int32_t width;
	int32_t height;
	int32_t stride;
};

struct cache_header {
	uint32_t version;
	uint32_t width;
	uint32_t height;
	uint32_t stride;
	const unsigned char *key;
	size_t key_length;
};

enum cache_result {
	CACHE_HIT,
	CACHE_MISS,
	// an entry failed its checks and was deleted
	CACHE_BAD,
	CACHE_FAILED,
};

void cache_key_add(struct cache_key *key, uint32_t value);

uint64_t cache_hash(const void *data, size_t length);

bool cache_header_parse(
	const unsigned char *data, size_t length, struct cache_header *header);

int cache_open(char *error, size_t error_size);

// reads straight into image->pixels, which hold garbage after a miss
enum cache_result cache_read(int dir, const struct cache_key *key,
	const struct cache_image *image, char *error, size_t error_size);

bool cache_write(int dir, const struct cache_key *key,
	const struct cache_image *image, char *error, size_t error_size);

#endif
