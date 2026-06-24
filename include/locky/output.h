#ifndef LOCKY_OUTPUT_H
#define LOCKY_OUTPUT_H

#include "locky/locky.h"
#include <stdint.h>

void locky_output_create(struct locky_state *state,
	struct wl_registry *registry, uint32_t global_name, uint32_t version);

void locky_output_destroy(struct locky_output *output);

#endif
