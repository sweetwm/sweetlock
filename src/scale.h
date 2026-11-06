#ifndef SWEETLOCK_SCALE_H
#define SWEETLOCK_SCALE_H

#include <stdbool.h>
#include <stdint.h>

// a scale is a numerator over 120, the fractional-scale-v1 encoding
enum {
	SCALE_DENOMINATOR = 120,
	// anything above this from the compositor is treated as bogus
	SCALE_MAX = 8 * SCALE_DENOMINATOR,
};

bool scale_valid(uint32_t scale);

int32_t scale_physical(uint32_t scale, int32_t logical);

// the smallest logical length that maps to whole physical pixels
int32_t scale_step(uint32_t scale);

double scale_factor(uint32_t scale);

#endif
