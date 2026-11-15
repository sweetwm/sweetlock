sweetlock(1)

# NAME

sweetlock - screen locker for Wayland

# SYNOPSIS

*sweetlock* [_options_]

*sweetlock* *--check* [_options_]

*sweetlock* *--preview* _FILE_ [_options_]

# DESCRIPTION

*sweetlock* locks the session through the _ext-session-lock-v1_ protocol and
unlocks it once PAM accepts the password. Every output is covered at once with
the base colour; the clock, date, avatar, name, password field and background
follow.

A compositor without _ext-session-lock-v1_ is refused. Each lock is one
process, which exits after the unlock.

# OPTIONS

Flags override the configuration file for one run. A bad or unknown flag is
logged and ignored; it never stops the screen from locking.

*-f*, *--daemonize*
	Return once the compositor has confirmed the lock, with status 0. The
	locker keeps running in the background. Use this from idle and
	before-sleep hooks, so the system sleeps already locked.

*--config* _PATH_
	Read the configuration from _PATH_ instead of the standard file.

*--no-config*
	Ignore the configuration file and use the built-in defaults.

*--set* _KEY_=_VALUE_
	Set any scalar key of *sweetlock*(5), for example
	*--set background.color=#000000*. Blobs can only be set in the file.

*--color* _HEX_
	Same as *--set background.color=*_HEX_.

*--image* _PATH_
	Same as *--set background.mode=image --set background.image.path=*_PATH_.

*--fit* _MODE_
	Same as *--set background.image.fit=*_MODE_.

*--dim* _N_
	Same as *--set background.image.dim=*_N_.

*--blur* _N_
	Same as *--set background.image.blur=*_N_.

*--no-clock*, *--no-date*, *--no-avatar*, *--no-username*
	Same as *--set clock.enabled=false* and so on.

*--check*
	Report problems with the configuration, the flags, the PAM service and
	the image paths (and the layout list with _input.layout_ = _"corner"_),
	then exit without locking. Exits 1 if the configuration, PAM or an
	image has a problem.

*--preview* _FILE_
	Draw the lock screen to the PNG _FILE_ instead of locking. Screenshots
	are denied while the session is locked, so this is how to see a
	configuration.

*--size* _WxH_
	Output size in pixels for *--preview*, each side from 1 to 8192. Default
	_1920x1080_.

*--scale* _S_
	Output scale for *--preview*, above 0 and at most 8. Default _1_.

*--preview-state* _STATE_
	Password field state for *--preview*: _idle_, _typing_, _revealed_,
	_checking_, _failed_, _info_, _error_, _caps_, _layout_, _cancel_ or
	_quiet_. Default _idle_.

*-h*, *--help*
	Show usage and exit.

*-V*, *--version*
	Show the version and exit.

# KEYS

Keys from every keyboard type into one password field. The keyboard layout,
dead keys and Compose work as usual.

*Enter*
	Check the password. On an empty field it starts PAM without one, for
	fingerprint readers and security keys, except while *Wrong password*
	shows.

*Backspace*
	Delete the last character.

*Escape*, *Ctrl+U*
	Clear the field. While a check has run for 10 seconds, *Escape* cancels
	it instead.

Clicking the eye in the field shows or hides the typed text. While a check
runs, other keys are ignored. PAM messages, *Caps Lock is on* and the keyboard
layout show on the line under the field.

# SIGNALS

*SIGUSR1*
	Unlock.

*SIGTERM*, *SIGINT*
	Exit without unlocking. The session stays locked, and the compositor
	keeps the screens covered.

*SIGHUP*
	Ignored, so closing the terminal that started the locker keeps the lock.

# EXIT STATUS

*0*
	The compositor confirmed the unlock. With *-f*, the screen is locked.

*1*
	Anything else, *SIGTERM* included. The session may still be locked. With
	*-f*, the lock failed, for example because the session was already
	locked.

# RECOVERY

If the password is not accepted, log in from another virtual terminal or over
SSH and unlock with:

	pkill -USR1 -xo sweetlock

If the locker crashed, the compositor keeps the screens covered. Start
*sweetlock* again to take over the lock.

# ENVIRONMENT

*SWEETLOCK_LOG*
	Log level: _error_, _warn_, _info_ (the default) or _debug_. The log goes
	to standard error.

*XDG_CONFIG_HOME*, *XDG_CACHE_HOME*
	Where the configuration file and the background cache live.

# FILES

_$XDG_CONFIG_HOME/sweetlock/config.toml_, _~/.config/sweetlock/config.toml_
	Configuration. See *sweetlock*(5).

_$XDG_CACHE_HOME/sweetlock/_, _~/.cache/sweetlock/_
	Rendered backgrounds and the avatar, so a repeated lock reads them back
	instead of drawing them again.

_/etc/pam.d/sweetlock_
	The PAM service. _/usr/local/etc/pam.d/sweetlock_ on FreeBSD. On NixOS,
	set _security.pam.services.sweetlock = {};_ instead.

# SEE ALSO

*sweetlock*(5), *pam*(8)
