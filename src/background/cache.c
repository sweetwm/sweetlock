#include "background/cache.h"

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "fs/path.h"

enum {
	MAGIC_SIZE = 8,
	NAME_LENGTH = 16,
	NAME_SIZE = NAME_LENGTH + sizeof ".tmp",
	SCAN_MAX = 64,
};

static const char magic[] = "sweetlck";
static_assert(sizeof magic == MAGIC_SIZE + 1, "magic size");

// %m, because strerror may share one buffer between threads
static bool fail_errno(char *error, size_t size, const char *what) {
	snprintf(error, size, "%s: %m", what);
	return false;
}

static void put_u32(unsigned char *out, uint32_t value) {
	for (int i = 0; i < 4; i++) {
		out[i] = (unsigned char)(value >> (8 * i));
	}
}

static uint32_t get_u32(const unsigned char *in) {
	return (uint32_t)in[0] | (uint32_t)in[1] << 8 | (uint32_t)in[2] << 16 |
	       (uint32_t)in[3] << 24;
}

void cache_key_add(struct cache_key *key, uint32_t value) {
	assert(key->length + 4 <= CACHE_KEY_MAX);
	put_u32(key->bytes + key->length, value);
	key->length += 4;
}

bool cache_header_parse(
	const unsigned char *data, size_t length, struct cache_header *header) {
	if (length < CACHE_HEADER_SIZE ||
		memcmp(data, magic, MAGIC_SIZE) != 0) {
		return false;
	}
	size_t key_length = get_u32(data + MAGIC_SIZE + 16);
	if (key_length > CACHE_KEY_MAX ||
		key_length > length - CACHE_HEADER_SIZE) {
		return false;
	}
	*header = (struct cache_header){
		.version = get_u32(data + MAGIC_SIZE),
		.width = get_u32(data + MAGIC_SIZE + 4),
		.height = get_u32(data + MAGIC_SIZE + 8),
		.stride = get_u32(data + MAGIC_SIZE + 12),
		.key = data + CACHE_HEADER_SIZE,
		.key_length = key_length,
	};
	return true;
}

static size_t header_write(unsigned char *out, const struct cache_key *key,
	const struct cache_image *image) {
	memcpy(out, magic, MAGIC_SIZE);
	put_u32(out + MAGIC_SIZE, CACHE_VERSION);
	put_u32(out + MAGIC_SIZE + 4, (uint32_t)image->width);
	put_u32(out + MAGIC_SIZE + 8, (uint32_t)image->height);
	put_u32(out + MAGIC_SIZE + 12, (uint32_t)image->stride);
	put_u32(out + MAGIC_SIZE + 16, (uint32_t)key->length);
	memcpy(out + CACHE_HEADER_SIZE, key->bytes, key->length);
	return CACHE_HEADER_SIZE + key->length;
}

// FNV-1a
uint64_t cache_hash(const void *data, size_t length) {
	const unsigned char *bytes = data;
	uint64_t hash = 0xcbf29ce484222325u;
	for (size_t i = 0; i < length; i++) {
		hash = (hash ^ bytes[i]) * 0x100000001b3u;
	}
	return hash;
}

// a collision only costs a redraw, since the header holds the key
static void entry_name(
	const struct cache_key *key, const char *suffix, char name[NAME_SIZE]) {
	snprintf(name, NAME_SIZE, "%016" PRIx64 "%.4s",
		cache_hash(key->bytes, key->length), suffix);
}

static bool is_entry(const char *name) {
	size_t length = strlen(name);
	if (length != NAME_LENGTH &&
		(length != NAME_SIZE - 1 ||
			strcmp(name + NAME_LENGTH, ".tmp") != 0)) {
		return false;
	}
	for (size_t i = 0; i < NAME_LENGTH; i++) {
		if (!strchr("0123456789abcdef", name[i])) {
			return false;
		}
	}
	return true;
}

static bool make_dir(const char *path, char *error, size_t size) {
	return mkdir(path, 0700) == 0 || errno == EEXIST ||
	       fail_errno(error, size, path);
}

int cache_open(char *error, size_t error_size) {
	char base[PATH_MAX];
	char path[PATH_MAX];
	if (!path_xdg(base, "XDG_CACHE_HOME", "/.cache") ||
		!path_join(path, base, "/sweetlock")) {
		snprintf(error, error_size,
			"neither XDG_CACHE_HOME nor HOME is a usable absolute "
			"path");
		return -1;
	}
	if (!make_dir(base, error, error_size) ||
		!make_dir(path, error, error_size)) {
		return -1;
	}
	int dir = open(path, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
	struct stat info;
	if (dir < 0 || fstat(dir, &info) != 0 ||
		(info.st_uid == geteuid() && (info.st_mode & 077) != 0 &&
			fchmod(dir, 0700) != 0)) {
		fail_errno(error, error_size, path);
	} else if (info.st_uid != geteuid()) {
		snprintf(error, error_size, "%s: not owned by this user", path);
	} else {
		return dir;
	}
	if (dir >= 0) {
		close(dir);
	}
	return -1;
}

static bool read_at(int fd, void *out, size_t length, off_t offset) {
	unsigned char *bytes = out;
	size_t total = 0;
	while (total < length) {
		ssize_t n = pread(fd, bytes + total, length - total,
			offset + (off_t)total);
		if (n < 0 && errno == EINTR) {
			continue;
		}
		if (n <= 0) {
			return false;
		}
		total += (size_t)n;
	}
	return true;
}

static bool matches(const unsigned char *data, size_t length,
	const struct cache_key *key, const struct cache_image *image) {
	struct cache_header header;
	return cache_header_parse(data, length, &header) &&
	       header.version == CACHE_VERSION &&
	       header.width == (uint32_t)image->width &&
	       header.height == (uint32_t)image->height &&
	       header.stride == (uint32_t)image->stride &&
	       header.key_length == key->length &&
	       memcmp(header.key, key->bytes, key->length) == 0;
}

static size_t pixel_bytes(const struct cache_image *image) {
	assert(image->width > 0 && image->height > 0 &&
		image->stride / 4 >= image->width);
	return (size_t)image->stride * (size_t)image->height;
}

// the header is checked against the expected one, and the length against
// both, before any pixels are read
enum cache_result cache_read(int dir, const struct cache_key *key,
	const struct cache_image *image, char *error, size_t error_size) {
	char name[NAME_SIZE];
	entry_name(key, "", name);
	// O_NONBLOCK so a FIFO planted at the name cannot hang the worker
	int fd = openat(dir, name,
		O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK | O_NOCTTY);
	if (fd < 0 && errno == ENOENT) {
		return CACHE_MISS;
	}
	if (fd < 0) {
		fail_errno(error, error_size, name);
		return CACHE_FAILED;
	}
	unsigned char header[CACHE_HEADER_SIZE + CACHE_KEY_MAX];
	size_t header_length = CACHE_HEADER_SIZE + key->length;
	size_t pixels = pixel_bytes(image);
	struct stat info;
	bool hit = fstat(fd, &info) == 0 && S_ISREG(info.st_mode) &&
		   (uintmax_t)info.st_size == header_length + pixels &&
		   read_at(fd, header, header_length, 0) &&
		   matches(header, header_length, key, image) &&
		   read_at(fd, image->pixels, pixels, (off_t)header_length);
	// the mtime orders entries for pruning, so a hit counts as a use
	if (hit) {
		futimens(fd, NULL);
	}
	close(fd);
	if (hit) {
		return CACHE_HIT;
	}
	unlinkat(dir, name, 0);
	snprintf(error, error_size, "deleted the bad cache entry %s", name);
	return CACHE_BAD;
}

static bool write_all(int fd, const void *data, size_t length) {
	const unsigned char *bytes = data;
	size_t total = 0;
	while (total < length) {
		ssize_t n = write(fd, bytes + total, length - total);
		if (n < 0 && errno == EINTR) {
			continue;
		}
		if (n < 0) {
			return false;
		}
		total += (size_t)n;
	}
	return true;
}

struct entry {
	char name[NAME_SIZE];
	struct timespec mtime;
};

static bool older(const struct timespec *a, const struct timespec *b) {
	return a->tv_sec < b->tv_sec ||
	       (a->tv_sec == b->tv_sec && a->tv_nsec < b->tv_nsec);
}

static size_t scan(int dir, struct entry *entries) {
	int copy = dup(dir);
	DIR *stream = copy >= 0 ? fdopendir(copy) : NULL;
	if (!stream) {
		if (copy >= 0) {
			close(copy);
		}
		return 0;
	}
	// the copy shares dir's offset, which an earlier scan left at the end
	rewinddir(stream);
	size_t count = 0;
	for (struct dirent *dirent = readdir(stream);
		dirent && count < SCAN_MAX; dirent = readdir(stream)) {
		struct stat info;
		if (is_entry(dirent->d_name) &&
			fstatat(dir, dirent->d_name, &info,
				AT_SYMLINK_NOFOLLOW) == 0 &&
			S_ISREG(info.st_mode)) {
			// is_entry bounds the length
			memcpy(entries[count].name, dirent->d_name,
				strlen(dirent->d_name) + 1);
			entries[count++].mtime = info.st_mtim;
		}
	}
	closedir(stream);
	return count;
}

// a leftover temporary file is old, so it goes first
static void prune(int dir) {
	struct entry entries[SCAN_MAX];
	size_t count = scan(dir, entries);
	while (count > CACHE_ENTRIES_MAX) {
		size_t oldest = 0;
		for (size_t i = 1; i < count; i++) {
			if (older(&entries[i].mtime, &entries[oldest].mtime)) {
				oldest = i;
			}
		}
		unlinkat(dir, entries[oldest].name, 0);
		entries[oldest] = entries[--count];
	}
}

bool cache_write(int dir, const struct cache_key *key,
	const struct cache_image *image, char *error, size_t error_size) {
	char name[NAME_SIZE];
	char temporary[NAME_SIZE];
	entry_name(key, "", name);
	entry_name(key, ".tmp", temporary);
	unsigned char header[CACHE_HEADER_SIZE + CACHE_KEY_MAX];
	size_t header_length = header_write(header, key, image);
	int fd = openat(dir, temporary,
		O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | O_NOFOLLOW |
			O_NOCTTY,
		0600);
	if (fd < 0) {
		return fail_errno(error, error_size, temporary);
	}
	// a crash after the rename must not leave a full-length entry of zeros
	bool written = write_all(fd, header, header_length) &&
		       write_all(fd, image->pixels, pixel_bytes(image)) &&
		       fdatasync(fd) == 0;
	if (!written) {
		fail_errno(error, error_size, temporary);
	}
	if (close(fd) != 0 && written) {
		written = fail_errno(error, error_size, temporary);
	}
	if (written && renameat(dir, temporary, dir, name) != 0) {
		written = fail_errno(error, error_size, name);
	}
	if (!written) {
		unlinkat(dir, temporary, 0);
		return false;
	}
	prune(dir);
	return true;
}
