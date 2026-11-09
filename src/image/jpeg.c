#include "image/jpeg.h"

#include <math.h>
#include <setjmp.h>
#include <stdio.h>
// jpeglib.h needs stdio.h first
#include <jpeglib.h>

enum {
	// progressive files have about ten; thousands only slow the decode down
	SCAN_MAX = 256,
};

// libjpeg's default error handler calls exit
struct failure {
	struct jpeg_error_mgr manager;
	jmp_buf jump;
	char message[JMSG_LENGTH_MAX];
};

_Noreturn static void stop(struct failure *failure, const char *message) {
	snprintf(failure->message, sizeof failure->message, "%s", message);
	longjmp(failure->jump, 1);
}

_Noreturn static void fail(j_common_ptr cinfo) {
	struct failure *failure = (struct failure *)cinfo->err;
	cinfo->err->format_message(cinfo, failure->message);
	longjmp(failure->jump, 1);
}

static void quiet(j_common_ptr cinfo, int level) {
	(void)cinfo;
	(void)level;
}

static void progress(j_common_ptr cinfo) {
	if (((j_decompress_ptr)cinfo)->input_scan_number > SCAN_MAX) {
		stop((struct failure *)cinfo->err,
			"too many progressive scans");
	}
}

// DCT scaling decodes at M/8 of the size for M in 1..8
static unsigned int scale_eighths(double fraction) {
	double eighths = ceil(fraction * 8);
	return eighths < 1 ? 1 : eighths > 8 ? 8 : (unsigned int)eighths;
}

static void decode(struct jpeg_decompress_struct *cinfo,
	struct failure *failure, cairo_surface_t *volatile *image,
	uint32_t width, uint32_t height, double fraction) {
	jpeg_read_header(cinfo, TRUE);
	if (cinfo->image_width != width || cinfo->image_height != height) {
		stop(failure, "libjpeg read a different size from the header");
	}
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	cinfo->out_color_space = JCS_EXT_XRGB;
#else
	cinfo->out_color_space = JCS_EXT_BGRX;
#endif
	cinfo->scale_num = scale_eighths(fraction);
	cinfo->scale_denom = 8;
	jpeg_start_decompress(cinfo);
	*image = cairo_image_surface_create(CAIRO_FORMAT_RGB24,
		(int)cinfo->output_width, (int)cinfo->output_height);
	if (cairo_surface_status(*image) != CAIRO_STATUS_SUCCESS) {
		stop(failure, "out of memory");
	}
	cairo_surface_flush(*image);
	unsigned char *pixels = cairo_image_surface_get_data(*image);
	size_t stride = (size_t)cairo_image_surface_get_stride(*image);
	while (cinfo->output_scanline < cinfo->output_height) {
		JSAMPROW row = pixels + cinfo->output_scanline * stride;
		if (jpeg_read_scanlines(cinfo, &row, 1) != 1) {
			stop(failure, "the decoder stopped early");
		}
	}
	jpeg_finish_decompress(cinfo);
	cairo_surface_mark_dirty(*image);
}

cairo_surface_t *image_jpeg_decode(const unsigned char *data, size_t length,
	uint32_t width, uint32_t height, double fraction, char *error,
	size_t error_size) {
	struct jpeg_decompress_struct cinfo = {0};
	struct failure failure = {.message = ""};
	struct jpeg_progress_mgr monitor = {.progress_monitor = progress};
	cairo_surface_t *volatile image = NULL;
	cinfo.err = jpeg_std_error(&failure.manager);
	failure.manager.error_exit = fail;
	failure.manager.emit_message = quiet;
	if (setjmp(failure.jump)) {
		snprintf(error, error_size, "failed to decode: %s",
			failure.message);
		jpeg_destroy_decompress(&cinfo);
		cairo_surface_destroy(image);
		return NULL;
	}
	jpeg_create_decompress(&cinfo);
	cinfo.progress = &monitor;
	jpeg_mem_src(&cinfo, data, length);
	decode(&cinfo, &failure, &image, width, height, fraction);
	jpeg_destroy_decompress(&cinfo);
	return image;
}
