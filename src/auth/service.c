#include "auth/service.h"

#include <limits.h>
#include <stdint.h>
#include <unistd.h>
#if defined(SWEETLOCK_TEST_PAM_CONFDIR) && defined(__FreeBSD__)
#include <security/openpam.h>
#endif

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

// where Linux-PAM and OpenPAM look
bool service_found(void) {
#ifdef SWEETLOCK_TEST_PAM_CONFDIR
	return service_in(SWEETLOCK_TEST_PAM_CONFDIR);
#elif defined(__FreeBSD__)
	return service_in("/etc/pam.d") || service_in("/usr/local/etc/pam.d");
#else
	return service_in("/etc/pam.d") || service_in("/usr/lib/pam.d");
#endif
}

int service_start(
	const char *user, const struct pam_conv *conv, pam_handle_t **pam) {
	*pam = NULL;
#if defined(SWEETLOCK_TEST_PAM_CONFDIR) && defined(__FreeBSD__)
	// OpenPAM has no confdir, but takes a path as the service once names
	// are unrestricted
	if (openpam_set_feature(OPENPAM_RESTRICT_SERVICE_NAME, 0) !=
		PAM_SUCCESS) {
		return PAM_SYSTEM_ERR;
	}
	return pam_start(
		SWEETLOCK_TEST_PAM_CONFDIR "/" SERVICE_NAME, user, conv, pam);
#elif defined(SWEETLOCK_TEST_PAM_CONFDIR)
	return pam_start_confdir(
		SERVICE_NAME, user, conv, SWEETLOCK_TEST_PAM_CONFDIR, pam);
#else
	return pam_start(SERVICE_NAME, user, conv, pam);
#endif
}
