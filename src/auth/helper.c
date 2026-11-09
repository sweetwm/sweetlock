#include "auth/helper.h"

#include <errno.h>
#include <security/pam_appl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include "auth/message.h"
#include "auth/service.h"
#include "input/password.h"
#include "log.h"
#include "utf8.h"

struct conversation {
	struct password *password;
	int fd;
	// a later hidden prompt asks for something else, such as an OTP code
	bool answered;
	// texts sent in this attempt
	int texts;
	bool no_service;
};

static void free_replies(struct pam_response *replies, int count) {
	for (int i = 0; i < count; i++) {
		if (replies[i].resp) {
			explicit_bzero(
				replies[i].resp, strlen(replies[i].resp));
			free(replies[i].resp);
		}
	}
	free(replies);
}

// a lost socket ends the attempt soon enough, so a failed send is ignored
static void tell(struct conversation *conversation, char kind, const char *text,
	size_t length) {
	if (conversation->texts >= MESSAGE_TEXT_COUNT) {
		if (conversation->texts == MESSAGE_TEXT_COUNT) {
			log_warn("PAM sent more than %d messages; not showing "
				 "the rest",
				MESSAGE_TEXT_COUNT);
			conversation->texts++;
		}
		return;
	}
	conversation->texts++;
	char packet[MESSAGE_REPLY_MAX];
	packet[0] = kind;
	memcpy(packet + 1, text, length);
	(void)send(conversation->fd, packet, length + 1, MSG_NOSIGNAL);
}

static void show(
	struct conversation *conversation, bool error, const char *text) {
	char line[MESSAGE_TEXT_MAX];
	size_t length = utf8_line(
		text, strnlen(text, PAM_MAX_MSG_SIZE), line, sizeof line);
	if (length == 0) {
		return;
	}
	if (error) {
		log_warn("PAM: %.*s", (int)length, line);
	} else {
		log_info("PAM: %.*s", (int)length, line);
	}
	tell(conversation, error ? MESSAGE_ERROR : MESSAGE_INFO, line, length);
}

_Static_assert(sizeof SERVICE_MISSING - 1 <= MESSAGE_TEXT_MAX,
	"the warning fits one text packet");

// before each attempt as well, since the locker clears it when one starts
static void warn_missing(struct conversation *conversation) {
	if (conversation->no_service) {
		tell(conversation, MESSAGE_ERROR, SERVICE_MISSING,
			sizeof SERVICE_MISSING - 1);
	}
}

static bool answer(const struct pam_message *message,
	struct pam_response *reply, struct conversation *conversation) {
	const char *text = message->msg ? message->msg : "";
	switch (message->msg_style) {
	case PAM_PROMPT_ECHO_OFF:
		if (conversation->answered) {
			log_warn("PAM asked for a second secret; failing the "
				 "attempt");
			return false;
		}
		conversation->answered = true;
		// PAM frees the answer, so it gets its own copy
		reply->resp = strdup(conversation->password->bytes);
		return reply->resp != NULL;
	case PAM_ERROR_MSG:
	case PAM_TEXT_INFO:
		show(conversation, message->msg_style == PAM_ERROR_MSG, text);
		return true;
	default:
		log_warn("PAM asked for something other than the password; "
			 "failing the attempt");
		return false;
	}
}

// Linux-PAM passes messages as an array of pointers
static int converse(int count, const struct pam_message **messages,
	struct pam_response **responses, void *data) {
	if (count <= 0 || count > PAM_MAX_NUM_MSG) {
		return PAM_CONV_ERR;
	}
	struct pam_response *replies = calloc((size_t)count, sizeof *replies);
	if (!replies) {
		return PAM_BUF_ERR;
	}
	for (int i = 0; i < count; i++) {
		if (!answer(messages[i], &replies[i], data)) {
			free_replies(replies, count);
			return PAM_CONV_ERR;
		}
	}
	*responses = replies;
	return PAM_SUCCESS;
}

static pam_handle_t *start(const char *user, const struct pam_conv *conv) {
	pam_handle_t *pam = NULL;
#ifdef SWEETLOCK_TEST_PAM_CONFDIR
	log_warn("built for tests: authenticating with the PAM config in %s",
		SWEETLOCK_TEST_PAM_CONFDIR);
	int status = pam_start_confdir(
		SERVICE_NAME, user, conv, SWEETLOCK_TEST_PAM_CONFDIR, &pam);
#else
	int status = pam_start(SERVICE_NAME, user, conv, &pam);
#endif
	if (status != PAM_SUCCESS) {
		log_error("failed to start PAM: %s", pam_strerror(pam, status));
		return NULL;
	}
	return pam;
}

static bool authenticate(pam_handle_t *pam, struct conversation *conversation) {
	conversation->answered = false;
	conversation->texts = 0;
	warn_missing(conversation);
	int status = pam_authenticate(pam, 0);
	if (status != PAM_SUCCESS) {
		// a wrong password is the usual failure; others point at the
		// setup
		if (status != PAM_AUTH_ERR) {
			log_warn("PAM: %s", pam_strerror(pam, status));
		}
		return false;
	}
	status = pam_setcred(pam, PAM_REFRESH_CRED);
	if (status != PAM_SUCCESS) {
		log_warn("PAM failed to refresh credentials: %s",
			pam_strerror(pam, status));
	}
	return true;
}

// MSG_TRUNC returns a packet's full size, so an oversized one is seen and
// refused
static void serve(
	int fd, pam_handle_t *pam, struct conversation *conversation) {
	struct password *password = conversation->password;
	for (;;) {
		ssize_t size = recv(
			fd, password->bytes, sizeof password->bytes, MSG_TRUNC);
		if (size < 0 && errno == EINTR) {
			continue;
		}
		if (size <= 0) {
			return;
		}
		bool valid = message_request(
			password->bytes, (size_t)size, &password->length);
		if (!valid) {
			log_error(
				"the PAM helper received a malformed request");
		}
		char reply = valid && authenticate(pam, conversation)
				     ? MESSAGE_SUCCESS
				     : MESSAGE_FAILURE;
		password_clear(password);
		if (send(fd, &reply, sizeof reply, MSG_NOSIGNAL) !=
			(ssize_t)sizeof reply) {
			return;
		}
	}
}

// PAM modules see only stdio and the socket, and the -f report socket must
// close when the locker exits
static void close_inherited(int fd) {
	if (fd > 3) {
		close_range(3, (unsigned)fd - 1, 0);
	}
	close_range(fd < 3 ? 3 : (unsigned)fd + 1, ~0U, 0);
}

_Noreturn void helper_run(int fd, pid_t parent, const char *user) {
	close_inherited(fd);
	// SIGTERM is blocked here as in the parent, so only SIGKILL works
	if (prctl(PR_SET_DUMPABLE, 0, 0, 0, 0) != 0 ||
		prctl(PR_SET_PDEATHSIG, SIGKILL, 0, 0, 0) != 0 ||
		getppid() != parent) {
		_exit(EXIT_FAILURE);
	}
	struct password *password = password_create();
	struct conversation conversation = {
		.password = password,
		.fd = fd,
		.no_service = !service_found(),
	};
	if (conversation.no_service) {
		log_error(SERVICE_MISSING_LOG);
		warn_missing(&conversation);
	}
	const struct pam_conv conv = {
		.conv = converse, .appdata_ptr = &conversation};
	pam_handle_t *pam = password ? start(user, &conv) : NULL;
	if (pam) {
		serve(fd, pam, &conversation);
		pam_end(pam, PAM_SUCCESS);
	}
	password_destroy(password);
	_exit(pam ? EXIT_SUCCESS : EXIT_FAILURE);
}
