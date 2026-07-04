#include "auth/pam.h"
#include "app/log.h"
#include "auth/comm.h"
#include <pwd.h>
#include <security/pam_appl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct conv_data {
	const char *password;
};

static int handle_conversation(int num_msg, const struct pam_message **msg,
	struct pam_response **resp, void *data) {
	struct conv_data *conv = data;
	if (num_msg <= 0) {
		return PAM_CONV_ERR;
	}

	struct pam_response *replies =
		calloc((size_t)num_msg, sizeof(*replies));
	if (replies == NULL) {
		return PAM_BUF_ERR;
	}

	for (int i = 0; i < num_msg; i++) {
		if (msg[i]->msg_style != PAM_PROMPT_ECHO_OFF &&
			msg[i]->msg_style != PAM_PROMPT_ECHO_ON) {
			continue; // PAM_ERROR_MSG / PAM_TEXT_INFO need no
				  // response
		}
		if (conv->password == NULL) {
			goto fail;
		}
		replies[i].resp = strdup(conv->password); // PAM frees this
		if (replies[i].resp == NULL) {
			goto fail;
		}
	}

	*resp = replies;
	return PAM_SUCCESS;

fail:
	for (int i = 0; i < num_msg; i++) {
		if (replies[i].resp != NULL) {
			explicit_bzero(
				replies[i].resp, strlen(replies[i].resp));
			free(replies[i].resp);
		}
	}
	free(replies);
	return PAM_CONV_ERR;
}

_Noreturn void locky_pam_run_child(void) {
	struct passwd *pw = getpwuid(getuid());
	if (pw == NULL || pw->pw_name == NULL) {
		locky_log(LOCKY_ERROR, "getpwuid failed");
		exit(EXIT_FAILURE);
	}

	struct conv_data conv_data = {0};
	const struct pam_conv conv = {
		.conv = handle_conversation,
		.appdata_ptr = &conv_data,
	};

	pam_handle_t *handle = NULL;
	if (pam_start("locky", pw->pw_name, &conv, &handle) != PAM_SUCCESS) {
		locky_log(LOCKY_ERROR, "pam_start failed");
		exit(EXIT_FAILURE);
	}

	int exit_status = EXIT_FAILURE;
	while (true) {
		char *password = NULL;
		ssize_t size = locky_comm_read_request(&password);
		if (size <= 0) {
			// 0 means the parent closed the channel: clean shutdown
			exit_status = size == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
			break;
		}

		conv_data.password = password;
		int rc = pam_authenticate(handle, 0);
		conv_data.password = NULL;
		locky_comm_free_request(password, (size_t)size);

		bool success = rc == PAM_SUCCESS;
		if (!success) {
			locky_log(LOCKY_ERROR, "authentication failed: %s",
				pam_strerror(handle, rc));
		}
		if (!locky_comm_write_reply(success)) {
			break;
		}
		if (success) {
			pam_setcred(handle, PAM_REFRESH_CRED);
			exit_status = EXIT_SUCCESS;
			break;
		}
	}

	pam_end(handle, PAM_SUCCESS);
	exit(exit_status);
}
