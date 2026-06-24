#include <stdio.h>
#include <wayland-client.h>

// bootstrap
int main(void) {
	struct wl_display *display = wl_display_connect(NULL);
	if (display == NULL) {
		fprintf(stderr,
			"locky: failed to connect to wayland display\n");
		return 1;
	}

	wl_display_disconnect(display);
	return 0;
}
