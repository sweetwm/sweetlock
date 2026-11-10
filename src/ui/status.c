#include "ui/status.h"

#include "auth/auth.h"
#include "config/config.h"
#include "input/input.h"
#include "ui/ui.h"

// PAM explains the attempt, Caps Lock fails it, a broken config can wait
// and the layout is only a hint
enum status_kind status_line(
	const struct ui_content *content, const char **text) {
	const struct auth *auth = content->auth;
	const struct input_status *keys = &content->input->status;
	if (auth->notice != AUTH_NOTICE_NONE) {
		*text = auth->notice_text;
		return auth->notice == AUTH_NOTICE_ERROR ? STATUS_PAM_ERROR
							 : STATUS_PAM_INFO;
	}
	if (keys->caps_lock) {
		*text = "Caps Lock is on";
		return STATUS_CAPS_LOCK;
	}
	if (content->notice && content->notice[0]) {
		*text = content->notice;
		return STATUS_NOTICE;
	}
	if (keys->layout[0] && content->config->input.layout == LAYOUT_STATUS) {
		*text = keys->layout;
		return STATUS_LAYOUT;
	}
	return STATUS_NONE;
}
