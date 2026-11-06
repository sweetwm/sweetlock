#include "scale.h"

#include <assert.h>

bool scale_valid(uint32_t scale) {
	return scale > 0 && scale <= SCALE_MAX;
}

int32_t scale_physical(uint32_t scale, int32_t logical) {
	assert(scale_valid(scale));
	int64_t value = (int64_t)logical * scale;
	int64_t half = SCALE_DENOMINATOR / 2;
	return (int32_t)((value >= 0 ? value + half : value - half) /
			 SCALE_DENOMINATOR);
}

int32_t scale_step(uint32_t scale) {
	assert(scale_valid(scale));
	uint32_t a = scale;
	uint32_t b = SCALE_DENOMINATOR;
	while (b != 0) {
		uint32_t r = a % b;
		a = b;
		b = r;
	}
	return (int32_t)(SCALE_DENOMINATOR / a);
}

double scale_factor(uint32_t scale) {
	return scale / (double)SCALE_DENOMINATOR;
}
