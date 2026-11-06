#ifndef SWEETLOCK_UI_USER_H
#define SWEETLOCK_UI_USER_H

#include <stddef.h>

enum { USER_NAME_SIZE = 256 };

void user_name(char name[USER_NAME_SIZE], const char *configured);

#endif
