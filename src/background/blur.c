#include "background/blur.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

enum { PASSES = 3 };

// sigma at the reduced size; the upscale of a blur this wide is smooth
static const double reduced_sigma = 3;

int32_t blur_factor(double sigma) {
	double factor = floor(sigma / reduced_sigma);
	return factor < 1 ? 1 : (int32_t)factor;
}

static void radii(double sigma, int32_t radius[PASSES]) {
	double variance = sigma * sigma;
	int32_t low = (int32_t)floor((sqrt(1 + 4 * variance) - 1) / 2);
	double step = 2.0 * (low + 1) / (double)PASSES;
	double wider = round((variance - (double)low * (low + 1)) / step);
	for (int i = 0; i < PASSES; i++) {
		radius[i] = low + (i < wider ? 1 : 0);
	}
}

static int32_t clamp(int32_t i, int32_t count) {
	return i < 0 ? 0 : i >= count ? count - 1 : i;
}

enum {
	// values per column strip in the vertical pass, so a strip stays in
	// cache
	STRIP = 64,
};

static void box(const uint16_t *in, uint16_t *out, int32_t count, int32_t lanes,
	int32_t radius) {
	uint32_t sum[STRIP] = {0};
	uint32_t width = 2 * (uint32_t)radius + 1;
	// sum / width, exact for the sums a box of 16-bit values can reach
	uint64_t inverse = (((uint64_t)1 << 32) + width - 1) / width;
	size_t row = (size_t)lanes;
	for (int32_t i = -radius; i <= radius; i++) {
		const uint16_t *sample = in + (size_t)clamp(i, count) * row;
		for (int32_t l = 0; l < lanes; l++) {
			sum[l] += sample[l];
		}
	}
	for (int32_t i = 0; i < count; i++) {
		const uint16_t *enter =
			in + (size_t)clamp(i + radius + 1, count) * row;
		const uint16_t *leave =
			in + (size_t)clamp(i - radius, count) * row;
		uint16_t *result = out + (size_t)i * row;
		for (int32_t l = 0; l < lanes; l++) {
			result[l] = (uint16_t)((sum[l] + width / 2) * inverse >>
					       32);
			sum[l] += (uint32_t)enter[l] - leave[l];
		}
	}
}

// line is count samples of lanes values; a and b each hold one such line
static void line_passes(uint16_t *line, uint16_t *a, uint16_t *b, int32_t count,
	int32_t lanes, const int32_t radius[PASSES]) {
	box(line, a, count, lanes, radius[0]);
	box(a, b, count, lanes, radius[1]);
	box(b, line, count, lanes, radius[2]);
}

// columns in strips of STRIP values, copied out and back
static void columns(uint16_t *pixels, int32_t width, int32_t height,
	uint16_t *scratch, const int32_t radius[PASSES]) {
	size_t row = (size_t)width * BLUR_CHANNELS;
	size_t size = (size_t)height * STRIP;
	uint16_t *strip = scratch;
	for (size_t x = 0; x < row; x += STRIP) {
		size_t lanes = row - x < STRIP ? row - x : STRIP;
		for (int32_t y = 0; y < height; y++) {
			memcpy(strip + (size_t)y * lanes,
				pixels + (size_t)y * row + x,
				lanes * sizeof *strip);
		}
		line_passes(strip, scratch + size, scratch + 2 * size, height,
			(int32_t)lanes, radius);
		for (int32_t y = 0; y < height; y++) {
			memcpy(pixels + (size_t)y * row + x,
				strip + (size_t)y * lanes,
				lanes * sizeof *strip);
		}
	}
}

bool blur(uint16_t *pixels, int32_t width, int32_t height, double sigma) {
	int32_t radius[PASSES];
	radii(sigma, radius);
	if (width < 1 || height < 1 || radius[0] < 1) {
		return true;
	}
	size_t row = (size_t)width * BLUR_CHANNELS;
	size_t strip = (size_t)height * STRIP;
	size_t line = row > strip ? row : strip;
	uint16_t *scratch = calloc(3 * line, sizeof *scratch);
	if (!scratch) {
		return false;
	}
	for (int32_t y = 0; y < height; y++) {
		line_passes(pixels + (size_t)y * row, scratch, scratch + line,
			width, BLUR_CHANNELS, radius);
	}
	columns(pixels, width, height, scratch, radius);
	free(scratch);
	return true;
}

static uint32_t *row_of(cairo_surface_t *surface, int32_t y) {
	return (uint32_t *)(cairo_image_surface_get_data(surface) +
			    (size_t)y * (size_t)cairo_image_surface_get_stride(
						surface));
}

// 8 bits to 16, so the blur and the upscale keep the fractions
static uint16_t *widen(cairo_surface_t *source, int32_t width, int32_t height) {
	uint16_t *wide = malloc(
		(size_t)width * (size_t)height * BLUR_CHANNELS * sizeof *wide);
	if (!wide) {
		return NULL;
	}
	for (int32_t y = 0; y < height; y++) {
		const uint32_t *in = row_of(source, y);
		uint16_t *out =
			wide + (size_t)y * (size_t)width * BLUR_CHANNELS;
		for (int32_t x = 0; x < width; x++) {
			for (int c = 0; c < BLUR_CHANNELS; c++) {
				out[x * BLUR_CHANNELS + c] =
					(uint16_t)((in[x] >> (8 * c) & 0xff)
						   << 8);
			}
		}
	}
	return wide;
}

// where an output pixel's centre falls between two source pixels
struct tap {
	int32_t from;
	int32_t to;
	// the weight of to, out of 256
	uint32_t weight;
};

static void taps(
	struct tap *out, int32_t count, int32_t factor, int32_t limit) {
	for (int32_t i = 0; i < count; i++) {
		double at = (i + 0.5) / factor - 0.5;
		double from = floor(at);
		out[i] = (struct tap){
			.from = clamp((int32_t)from, limit),
			.to = clamp((int32_t)from + 1, limit),
			.weight = (uint32_t)lround((at - from) * 256),
		};
	}
}

// an ordered dither, so the 16-bit blur reaches 8 bits without bands
static const uint8_t bayer[4][4] = {
	{0, 8, 2, 10},
	{12, 4, 14, 6},
	{3, 11, 1, 9},
	{15, 7, 13, 5},
};

// across is the source row already mixed between its two rows
static void scale_row(uint32_t *out, const uint32_t *across_row,
	const struct tap *across, int32_t width, int32_t y) {
	for (int32_t x = 0; x < width; x++) {
		struct tap t = across[x];
		const uint32_t *from =
			across_row + (size_t)t.from * BLUR_CHANNELS;
		const uint32_t *to = across_row + (size_t)t.to * BLUR_CHANNELS;
		uint32_t threshold = bayer[y & 3][x & 3] * 16u + 8;
		uint32_t pixel = 0;
		for (int c = 0; c < BLUR_CHANNELS; c++) {
			uint32_t value = (from[c] * (256 - t.weight) +
						 to[c] * t.weight + 128) >>
					 8;
			uint32_t byte = (value + threshold) >> 8;
			pixel |= (byte > 255 ? 255 : byte) << (8 * c);
		}
		out[x] = pixel;
	}
}

static void mix_rows(uint32_t *out, const uint16_t *top, const uint16_t *bottom,
	uint32_t weight, size_t count) {
	for (size_t i = 0; i < count; i++) {
		out[i] = (top[i] * (256 - weight) + bottom[i] * weight + 128) >>
			 8;
	}
}

static bool scale_up(const uint16_t *wide, int32_t width, int32_t height,
	int32_t factor, cairo_surface_t *target) {
	int32_t out_width = cairo_image_surface_get_width(target);
	int32_t out_height = cairo_image_surface_get_height(target);
	size_t row = (size_t)width * BLUR_CHANNELS;
	struct tap *across = calloc((size_t)out_width, sizeof *across);
	struct tap *down = calloc((size_t)out_height, sizeof *down);
	uint32_t *mixed = calloc(row, sizeof *mixed);
	bool ok = across && down && mixed;
	if (ok) {
		taps(across, out_width, factor, width);
		taps(down, out_height, factor, height);
		cairo_surface_flush(target);
		for (int32_t y = 0; y < out_height; y++) {
			mix_rows(mixed, wide + (size_t)down[y].from * row,
				wide + (size_t)down[y].to * row, down[y].weight,
				row);
			scale_row(
				row_of(target, y), mixed, across, out_width, y);
		}
		cairo_surface_mark_dirty(target);
	}
	free(across);
	free(down);
	free(mixed);
	return ok;
}

bool blur_scale(cairo_surface_t *source, double sigma, int32_t factor,
	cairo_surface_t *target) {
	cairo_surface_flush(source);
	int32_t width = cairo_image_surface_get_width(source);
	int32_t height = cairo_image_surface_get_height(source);
	uint16_t *wide = widen(source, width, height);
	bool ok = wide && blur(wide, width, height, sigma) &&
		  scale_up(wide, width, height, factor, target);
	free(wide);
	return ok;
}
