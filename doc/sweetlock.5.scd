sweetlock(5)

# NAME

sweetlock - configuration file for the sweetlock screen locker

# DESCRIPTION

*sweetlock*(1) reads _$XDG_CONFIG_HOME/sweetlock/config.toml_, or
_~/.config/sweetlock/config.toml_ when it is unset. The file is TOML and
optional; every key has a default.

The file is applied only when every key in it is valid. An unknown key, a wrong
type or a value out of range ignores the whole file: the screen still locks
with the defaults and the flags, and shows *config ignored:* under the field.
*sweetlock --check* reports every problem without locking.

Every key except the blobs can also be set with *--set* _KEY_=_VALUE_, which
wins over the file.

Colours are _"#rrggbb"_ or _"#rrggbbaa"_. Paths are absolute or start with
_~/_, at most 4095 bytes. Other strings are at most 127 bytes of UTF-8 without
control characters.

# BACKGROUND

*background.mode*
	_"solid"_, _"gradient"_, _"radial"_, _"layered"_ or _"image"_. Default
	_"solid"_. Anything but solid is drawn once per output size and cached in
	_$XDG_CACHE_HOME/sweetlock/_.

*background.color*
	The solid colour, and the first frame in every mode. Default
	_"#1f1b2e"_. Backgrounds are opaque, so alpha is ignored here and in the
	gradient and radial colours.

*background.gradient.top_left*, *top_right*, *bottom_left*, *bottom_right*
	Corner colours, blended between them. Defaults _"#2b2342"_, _"#1f1b2e"_,
	_"#1f1b2e"_, _"#2b2342"_.

*background.radial.center*, *background.radial.edge*
	Centre and edge colours. Defaults _"#373057"_ and _"#1f1b2e"_.

*background.radial.center_x*, *background.radial.center_y*
	Centre, 0 to 100 percent of the width and height. Default _50_.

*background.radial.radius*
	1 to 200 percent of the distance from the centre to the farthest corner.
	Default _80_.

*background.layered.base*
	_"solid"_, _"gradient"_ or _"radial"_, drawn from the keys above, under
	the blobs. Default _"radial"_.

*[[background.layered.blob]]*
	Up to three soft round blobs over the base. A blob list in the file
	replaces the default two, and _blob = []_ removes them. Each has:

	*color*: its colour; the alpha is its strength. Default _"#ffffff2e"_.

	*x*, *y*: the centre, 0 to 100 percent of the output. Default _50_.

	*size*: the radius, 1 to 100 percent of the output. Default _36_.

*background.image.path*
	A PNG or JPEG wallpaper for the _"image"_ mode. Default empty. A missing
	or broken file keeps *background.color*.

*background.image.fit*
	_"fill"_ covers the output and crops, _"fit"_ shows all of it on
	*background.color*, _"center"_ puts one image pixel on each output pixel,
	_"stretch"_ ignores the aspect ratio. Default _"fill"_.

*background.image.dim*
	Darken the image, 0 to 100 percent. Default _0_.

*background.image.blur*
	Blur strength in logical pixels, 0 to 100. Default _0_, off.

# CLOCK AND DATE

*clock.enabled*, *date.enabled*
	Show the clock and the date. Default _true_.

*clock.format*
	A *strftime*(3) format. Default _"%H:%M"_. With seconds, the clock ticks
	every second.

*date.format*
	A *strftime*(3) format. Default _"%A, %B %-d"_. Day and month names
	follow *LC_TIME*.

# AVATAR AND USERNAME

*avatar.enabled*
	Show the avatar. Default _true_.

*avatar.path*
	A PNG or JPEG. Default empty, which tries
	_/var/lib/AccountsService/icons/LOGIN_, then _~/.face_, then draws an
	outline.

*username.enabled*
	Show the name. Default _true_.

*username.text*
	The name shown. Default empty, which uses the full name from the account,
	then the login name.

With both the avatar and the username off, the clock and date move just above
the password field.

# INPUT

*input.placeholder*
	Text in the empty field. Default _"Password"_.

*input.reveal_toggle*
	Show the eye that reveals the typed text. Default _true_.

*input.layout*
	Where the keyboard layout shows when the keymap has more than one:
	_"status"_ puts its name under the field, _"corner"_ its code in a badge
	at the top right. Default _"status"_.

*input.hide_after*
	Seconds without input before the avatar, name and field hide, up to 3600.
	Default _0_, never. The first key shows them again and is typed, unless
	it is *Escape*. Hiding wipes a half-typed password.

# UI

*ui.output*
	The one output that shows the clock and the login, for example _"DP-1"_.
	The others show only the background. Default empty, every output, as
	does a name no connected output has.

# SCREEN OFF

*screen_off.timeout*
	Seconds without input before every output powers off, up to 3600.
	Default _0_, never. A key or click powers them back on and is not typed.
	Unlocking powers them on too. If another tool powers them on, the
	timeout starts over. Needs the compositor's
	_wlr-output-power-management-unstable-v1_.

# THEME

*theme.font*
	A fontconfig family, or a comma-separated list. Default _"Noto Sans"_.

*theme.clock_font*
	Font for the clock and date. Default empty, which uses *theme.font*.

*theme.clock_weight*
	Clock weight, 100 to 1000; 400 is regular, 700 bold. The date is 100
	lighter. Default _700_.

*theme.foreground*
	The username, the avatar outline, the field's text and *Caps Lock is on*.
	Default _"#e8e6f0"_.

*theme.muted*
	The clock, the date, the eye, the checking state and PAM messages.
	Default _"#a9a4bd"_.

*theme.field*
	The password field, the avatar's circle and the layout badge. Default
	_"#ffffff1a"_.

*theme.error*
	A wrong password, PAM errors and *config ignored:*. Default _"#e06c75"_.

# EXAMPLE

```
[background]
mode = "image"

[background.image]
path = "~/Pictures/wallpaper.jpg"
dim = 20
blur = 12

[screen_off]
timeout = 60
```

# SEE ALSO

*sweetlock*(1), *strftime*(3)
