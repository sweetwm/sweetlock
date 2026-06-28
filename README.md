# Locky

A minimal, modern Wayland screen locker.

Locky is a small, daemon-free screen locker built as a Wayland client on top of
the `ext-session-lock-v1` protocol, with Cairo rendering and PAM authentication.
It aims for a polished default look with a tiny set of configuration options.

## Status

Early development, but functional end to end: Locky acquires the Wayland
session lock, draws the default clock/user/password UI, accepts keyboard input,
authenticates with PAM, and unlocks only after successful authentication.

## Building

```sh
meson setup build
ninja -C build
meson test -C build
```

## License

MIT
