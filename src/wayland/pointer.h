#ifndef SWEETLOCK_WAYLAND_POINTER_H
#define SWEETLOCK_WAYLAND_POINTER_H

struct display;
struct pointer;
struct wl_pointer;

struct pointer *pointer_create(struct display *display, struct wl_pointer *wl);

void pointer_destroy(struct pointer *pointer);

#endif
