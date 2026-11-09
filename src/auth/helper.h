#ifndef SWEETLOCK_AUTH_HELPER_H
#define SWEETLOCK_AUTH_HELPER_H

// the hidden mode the locker spawns itself in: sweetlock --pam-helper LOGIN
#define HELPER_FLAG "--pam-helper"

// the helper's end of the socket
enum { HELPER_FD = 3 };

// answers requests on HELPER_FD until the parent goes away, then exits
_Noreturn void helper_main(const char *user);

#endif
