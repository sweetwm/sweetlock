# Locky

A minimal, modern Wayland screen locker.

Locky is a small, daemon-free screen locker built as a Wayland client on top of
the `ext-session-lock-v1` protocol, with Cairo rendering and PAM authentication.
It aims for a polished default look with a tiny set of configuration options.

## Status

Early development. The current build is a minimal bootstrap that connects to the
Wayland display and exits.

## Building

```sh
meson setup build
ninja -C build
```

## License

MIT
