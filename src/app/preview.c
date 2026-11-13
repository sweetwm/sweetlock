#include <stdlib.h>
#include <string.h>

#include "app/app.h"
#include "auth/auth.h"
#include "background/draw.h"
#include "background/job.h"
#include "input/input.h"
#include "input/password.h"
#include "log.h"
#include "ui/clock.h"
#include "ui/field.h"
#include "ui/ui.h"
#include "ui/user.h"
#include "wayland/scale.h"

static bool draw_part(struct ui *ui, enum ui_part part, cairo_t *cr,
	int32_t width, int32_t height) {
	struct ui_rect rect;
	if (!ui_shows(ui->content, part) || ui_hidden(ui->content, part) ||
		!ui_place(ui->content, part, width, height, ui->scale, &rect)) {
		return true;
	}
	int32_t physical_width = scale_physical(ui->scale, rect.width);
	cairo_save(cr);
	cairo_translate(cr, scale_physical(ui->scale, rect.x),
		scale_physical(ui->scale, rect.y));
	// the same bounds as the part's own buffer
	cairo_rectangle(cr, 0, 0, physical_width,
		scale_physical(ui->scale, rect.height));
	cairo_clip(cr);
	bool ok = ui_draw(ui, part, cr, &rect);
	cairo_restore(cr);
	return ok;
}

static bool draw(cairo_t *cr, const struct ui_content *content,
	const struct preview *preview) {
	char error[BACKGROUND_ERROR_SIZE] = "";
	cairo_surface_t *target = cairo_get_target(cr);
	// as when locked, a failed image leaves the base colour
	if (!background_paint(content->config, preview->scale, target, error,
		    sizeof error)) {
		log_warn("background: %s; keeping the base colour", error);
		if (!background_draw(content->config, target)) {
			return false;
		}
	}
	struct ui *ui = ui_create(content, preview->scale);
	if (!ui) {
		return false;
	}
	// the logical size a compositor would configure for this output
	int32_t width = (int32_t)(((int64_t)preview->width * SCALE_DENOMINATOR +
					  preview->scale / 2) /
				  preview->scale);
	int32_t height =
		(int32_t)(((int64_t)preview->height * SCALE_DENOMINATOR +
				  preview->scale / 2) /
			  preview->scale);
	bool ok = true;
	for (int i = 0; ok && i < UI_PARTS; i++) {
		ok = draw_part(ui, (enum ui_part)i, cr, width, height);
	}
	ui_destroy(ui);
	return ok && cairo_status(cr) == CAIRO_STATUS_SUCCESS;
}

static int write_png(
	const struct ui_content *content, const struct preview *preview) {
	cairo_surface_t *surface = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, preview->width, preview->height);
	cairo_t *cr = cairo_create(surface);
	bool drawn = draw(cr, content, preview);
	cairo_destroy(cr);
	cairo_status_t status =
		drawn ? cairo_surface_write_to_png(surface, preview->path)
		      : CAIRO_STATUS_SUCCESS;
	cairo_surface_destroy(surface);
	if (!drawn) {
		log_error("failed to draw the preview");
		return EXIT_FAILURE;
	}
	if (status != CAIRO_STATUS_SUCCESS) {
		log_error("failed to write the preview to %s: %s",
			preview->path, cairo_status_to_string(status));
		return EXIT_FAILURE;
	}
	log_info("wrote a %dx%d preview at scale %.3f to %s", preview->width,
		preview->height, scale_factor(preview->scale), preview->path);
	return EXIT_SUCCESS;
}

// sample lines as pam_fprintd words them
static void show_notice(struct auth *auth, enum preview_field state) {
	static const char info[] =
		"Place your finger on the fingerprint reader";
	static const char error[] = "Failed to match fingerprint";
	if (state == PREVIEW_INFO || state == PREVIEW_CANCEL) {
		auth->notice = AUTH_NOTICE_INFO;
		memcpy(auth->notice_text, info, sizeof info);
	} else if (state == PREVIEW_ERROR) {
		auth->notice = AUTH_NOTICE_ERROR;
		memcpy(auth->notice_text, error, sizeof error);
	}
}

int app_preview(const struct config *config, const char *notice,
	const struct preview *preview) {
	static const char sample[] = "sweetlock";
	enum preview_field state = preview->field;
	struct password *password = password_create();
	if (!password) {
		return EXIT_FAILURE;
	}
	if (state == PREVIEW_TYPING || state == PREVIEW_REVEALED) {
		password_append(password, sample, sizeof sample - 1);
	}
	struct input input = {
		.password = password,
		.repeat_fd = -1,
		.revealed = state == PREVIEW_REVEALED,
		.status = {.caps_lock = state == PREVIEW_CAPS},
		.quiet = state == PREVIEW_QUIET,
	};
	if (state == PREVIEW_LAYOUT) {
		static const char layout[] = "Latvian (apostrophe)";
		memcpy(input.status.layout, layout, sizeof layout);
	}
	struct auth auth = {
		.pid = -1,
		.fd = -1,
		.timer_fd = -1,
		.busy = state == PREVIEW_CHECKING || state == PREVIEW_INFO ||
			state == PREVIEW_CANCEL,
		.cancellable = state == PREVIEW_CANCEL,
	};
	struct field field = {
		.timer_fd = -1,
		.failed = state == PREVIEW_FAILED || state == PREVIEW_ERROR,
	};
	show_notice(&auth, state);
	struct clock clock = {.fd = -1};
	struct user user;
	user_lookup(&user, config->username.text);
	struct ui_content content = {
		.config = config,
		.input = &input,
		.auth = &auth,
		.field = &field,
		.user = &user,
		.notice = notice,
	};
	ui_content_start(&content, &clock);
	ui_content_load_avatar(&content);
	int status = write_png(&content, preview);
	ui_content_finish(&content);
	clock_stop(&clock);
	password_destroy(password);
	return status;
}
