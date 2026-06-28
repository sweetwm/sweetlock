#include "locky/locky.h"
#include "locky/password.h"
#include <assert.h>
#include <string.h>

static int auth_submits;
static int render_calls;

void locky_auth_submit(struct locky_state *state) {
	(void)state;
	auth_submits++;
}

void locky_render_all_outputs(struct locky_state *state) {
	(void)state;
	render_calls++;
}

static void assert_zeroed(const char *buf, size_t from, size_t to) {
	for (size_t i = from; i < to; i++) {
		assert(buf[i] == '\0');
	}
}

static void press(
	struct locky_state *state, xkb_keysym_t keysym, uint32_t codepoint) {
	locky_keyboard_handle_key(state, keysym, codepoint);
}

static void test_append_length_and_backspace_zeroing(void) {
	struct locky_state state = {0};
	assert(locky_password_init(&state.password));

	press(&state, XKB_KEY_a, 'a');
	press(&state, XKB_KEY_eacute, 0x00E9);
	press(&state, XKB_KEY_NoSymbol, 0x1F642);

	assert(state.password.len == 7);
	assert(locky_password_length(&state.password) == 3);
	assert(memcmp(state.password.buffer, "a\xC3\xA9\xF0\x9F\x99\x82", 7) ==
		0);

	press(&state, XKB_KEY_BackSpace, 0);
	assert(state.password.len == 3);
	assert(locky_password_length(&state.password) == 2);
	assert(memcmp(state.password.buffer, "a\xC3\xA9", 3) == 0);
	assert_zeroed(state.password.buffer, 3, 8);

	locky_password_finish(&state.password);
}

static void test_clear_paths_zero_secret_bytes(void) {
	struct locky_state state = {0};
	assert(locky_password_init(&state.password));

	press(&state, XKB_KEY_p, 'p');
	press(&state, XKB_KEY_w, 'w');
	assert(state.password.len == 2);

	press(&state, XKB_KEY_Escape, 0);
	assert(state.password.len == 0);
	assert_zeroed(state.password.buffer, 0, 8);

	press(&state, XKB_KEY_x, 'x');
	state.xkb.control = true;
	press(&state, XKB_KEY_u, 'u');
	assert(state.password.len == 0);
	assert_zeroed(state.password.buffer, 0, 8);

	locky_password_finish(&state.password);
}

static void test_auth_submit_and_invalid_reset(void) {
	struct locky_state state = {0};
	assert(locky_password_init(&state.password));

	press(&state, XKB_KEY_p, 'p');
	press(&state, XKB_KEY_Return, 0);
	assert(auth_submits == 1);

	state.auth_state = LOCKY_AUTH_INVALID;
	press(&state, XKB_KEY_q, 'q');
	assert(state.auth_state == LOCKY_AUTH_IDLE);
	assert(state.password.len == 2);

	locky_password_finish(&state.password);
}

int main(void) {
	test_append_length_and_backspace_zeroing();
	test_clear_paths_zero_secret_bytes();
	test_auth_submit_and_invalid_reset();
	assert(render_calls > 0);
	return 0;
}
