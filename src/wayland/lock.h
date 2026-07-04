#ifndef LOCKY_LOCK_H
#define LOCKY_LOCK_H

#include "app/state.h"
#include <stdbool.h>

bool locky_lock_acquire(struct locky_state *state);

void locky_lock_surface_create(struct locky_output *output);

void locky_lock_release(struct locky_state *state);

#endif
