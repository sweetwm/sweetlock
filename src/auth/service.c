#include "auth/service.h"

#include <limits.h>
#include <stdint.h>
#include <unistd.h>

#include "fs/file.h"
#include "fs/path.h"

bool service_in(const char *dir) {
	char path[PATH_MAX];
	if (!path_join(path, dir, "/" SERVICE_NAME)) {
		return false;
	}
	int fd = -1;
	struct stat info;
	char error[64];
	if (file_open(path, SIZE_MAX, &fd, &info, error, sizeof error) !=
		FILE_OK) {
		return false;
	}
	close(fd);
	return true;
}

bool service_found(void) {
#ifdef SWEETLOCK_TEST_PAM_CONFDIR
	return service_in(SWEETLOCK_TEST_PAM_CONFDIR);
#else
	return service_in("/etc/pam.d") || service_in("/usr/lib/pam.d");
#endif
}
