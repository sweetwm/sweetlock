#ifndef SWEETLOCK_AUTH_HELPER_H
#define SWEETLOCK_AUTH_HELPER_H

#include <sys/types.h>

// the body of the forked PAM helper: answers requests on fd until the parent
// goes away, then exits
_Noreturn void helper_run(int fd, pid_t parent, const char *user);

#endif
