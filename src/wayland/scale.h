#ifndef SWEETLOCK_WAYLAND_SCALE_H
#define SWEETLOCK_WAYLAND_SCALE_H

#include <stdbool.h>
#include <stdint.h>

// a scale is a numerator over 120, the fractional-scale-v1 encoding
enum {
	SCALE_DENOMINATOR = 120,
	// anything above this from the compositor is treated as bogus
	SCALE_MAX = 8 * SCALE_DENOMINATOR,
	// longer logical sides get no UI and no background
	SCALE_SIDE_MAX = 1 << 16,
};

_Static_assert(SCALE_SIDE_MAX <= INT32_MAX / (SCALE_MAX / SCALE_DENOMINATOR),
	"scale_physical must fit every bounded side");

bool scale_valid(uint32_t scale);

int32_t scale_physical(uint32_t scale, int32_t logical);

// the smallest logical length that maps to whole physical pixels
int32_t scale_step(uint32_t scale);

double scale_factor(uint32_t scale);

#endif
