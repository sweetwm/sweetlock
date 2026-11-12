#ifndef SWEETLOCK_INPUT_INPUT_H
#define SWEETLOCK_INPUT_INPUT_H

#include <stdbool.h>
#include <stdint.h>

struct auth;
struct field;
struct keyboard;
struct password;
struct xkb_compose_table;
struct xkb_context;
struct xkb_state;

// the longest xkeyboard-config description is about 60 bytes
enum { INPUT_LAYOUT_SIZE = 128 };

struct input_status {
	bool caps_lock;
	char layout[INPUT_LAYOUT_SIZE];
};

struct input {
	struct auth *auth;
	const struct field *field;
	struct xkb_context *xkb;
	struct xkb_compose_table *compose;
	bool compose_loaded;
	bool ready;
	struct password *password;
	bool revealed;
	struct input_status status;
	bool changed;
	// apart from changed, since the layout may have its own part
	bool layout_changed;
	int64_t active_ns;
	// the display keeps this current from the outputs' power modes
	bool screens_off;
	bool wake;
	int repeat_fd;
	struct keyboard *repeat_keyboard;
	uint32_t repeat_key;
};

bool input_init(
	struct input *input, struct auth *auth, const struct field *field);

void input_finish(struct input *input);

void input_press(struct input *input, struct keyboard *keyboard, uint32_t key);

void input_release(
	struct input *input, const struct keyboard *keyboard, uint32_t key);

// stops a repeat from this keyboard: on leave, a new keymap or its destroy
void input_forget(struct input *input, const struct keyboard *keyboard);

// the repeat timer is readable
void input_repeat(struct input *input);

// pointer motion, a scroll or a button
void input_touch(struct input *input);

// true when the press only asks for the screens back on
bool input_wakes(struct input *input);

// wipes the password and hides it, as the screens go off
void input_clear(struct input *input);

// a key press or a modifier change on a keyboard with this state
void input_status(struct input *input, struct xkb_state *state);

void input_toggle_reveal(struct input *input);

#endif
