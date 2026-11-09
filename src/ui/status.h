#ifndef SWEETLOCK_UI_STATUS_H
#define SWEETLOCK_UI_STATUS_H

struct ui_content;

enum status_kind {
	STATUS_NONE,
	STATUS_PAM_INFO,
	STATUS_PAM_ERROR,
	STATUS_CAPS_LOCK,
	STATUS_NOTICE,
	STATUS_LAYOUT,
};

enum status_kind status_line(
	const struct ui_content *content, const char **text);

#endif
