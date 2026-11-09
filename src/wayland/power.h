#ifndef SWEETLOCK_WAYLAND_POWER_H
#define SWEETLOCK_WAYLAND_POWER_H

#include <stdbool.h>

struct display;
struct output;

// after locked; false when the compositor offers no power control
bool power_start(struct display *display);

void power_attach(struct output *output);

void power_detach(struct output *output);

void power_set(struct display *display, bool on);

// keeps input.screens_off true while any output is off
void power_update(struct display *display);

#endif
