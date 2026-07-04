#ifndef LOCKY_AUTH_H
#define LOCKY_AUTH_H

#include "app/state.h"
#include <stdbool.h>

bool locky_auth_init(void);

void locky_auth_submit(struct locky_state *state);

int locky_auth_fd(void);

void locky_auth_handle_reply(struct locky_state *state);

void locky_auth_finish(void);

#endif
