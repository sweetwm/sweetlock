project(
	'sweetlock',
	'c',
	version: '0.1.0',
	license: 'GPL-3.0-or-later',
	meson_version: '>= 1.1.0',
	default_options: [
		'c_std=c17',
		'warning_level=2',
	],
)

cc = meson.get_compiler('c')

add_project_arguments(
	'-DSWEETLOCK_VERSION="' + meson.project_version() + '"',
	# memfd_create, signalfd, timerfd and the wider Linux surface
	'-D_GNU_SOURCE',
	cc.get_supported_arguments(
		'-Wshadow',
		'-Wconversion',
		'-Wno-sign-conversion',
		'-Wmissing-prototypes',
		'-Wstrict-prototypes',
	),
	language: 'c',
)

fs = import('fs')

cairo = dependency('cairo')
# system includes, so GLib's headers stay out of our warning flags
pangocairo = dependency('pangocairo', include_type: 'system')
libm = cc.find_library('m', required: false)
threads = dependency('threads')
wayland_client = dependency('wayland-client')
# cursor-shape-v1 arrived in 1.32
wayland_protocols = dependency('wayland-protocols', version: '>= 1.32')
wayland_scanner = find_program(
	dependency('wayland-scanner', native: true).get_variable('wayland_scanner'),
	native: true,
)

xkbcommon = dependency('xkbcommon')
pam = dependency('pam')
libjpeg = dependency('libjpeg')

subdir('protocol')

# vendored TOML parser: our sanitizers and fuzzer instrument it, but it is not
# written to our warning flags
tomlc17_dep = declare_dependency(
	link_with: static_library(
		'tomlc17',
		'third_party/tomlc17/tomlc17.c',
		c_args: '-w',
	),
	include_directories: include_directories(
		'third_party/tomlc17',
		is_system: true,
	),
)

sweetlock_inc = include_directories('src')

sweetlock_args = []
test_pam_confdir = get_option('test_pam_confdir')
if test_pam_confdir != ''
	warning('test_pam_confdir is set: this sweetlock authenticates against '
		+ test_pam_confdir + ', not the system PAM config')
	sweetlock_args += '-DSWEETLOCK_TEST_PAM_CONFDIR="' + test_pam_confdir + '"'
endif

executable(
	'sweetlock',
	'src/main.c',
	'src/app/check.c',
	'src/app/daemon.c',
	'src/app/preview.c',
	'src/app/run.c',
	'src/app/screen.c',
	'src/app/signals.c',
	'src/app/worker.c',
	'src/auth/auth.c',
	'src/auth/helper.c',
	'src/auth/message.c',
	'src/auth/service.c',
	'src/background/blur.c',
	'src/background/cache.c',
	'src/background/draw.c',
	'src/background/job.c',
	'src/background/wallpaper.c',
	'src/config/args.c',
	'src/config/load.c',
	'src/config/parse.c',
	'src/config/schema.c',
	'src/fs/file.c',
	'src/fs/path.c',
	'src/image/image.c',
	'src/image/jpeg.c',
	'src/input/input.c',
	'src/input/key.c',
	'src/input/keyboard.c',
	'src/input/layout.c',
	'src/input/password.c',
	'src/log.c',
	'src/render/icon.c',
	'src/render/text.c',
	'src/ui/avatar.c',
	'src/ui/clock.c',
	'src/ui/field.c',
	'src/ui/status.c',
	'src/ui/draw.c',
	'src/ui/place.c',
	'src/ui/ui.c',
	'src/ui/user.c',
	'src/utf8.c',
	'src/wayland/backdrop.c',
	'src/wayland/buffer.c',
	'src/wayland/display.c',
	'src/wayland/lock.c',
	'src/wayland/output.c',
	'src/wayland/panel.c',
	'src/wayland/pointer.c',
	'src/wayland/power.c',
	'src/wayland/scale.c',
	'src/wayland/seat.c',
	c_args: sweetlock_args,
	include_directories: sweetlock_inc,
	dependencies: [
		tomlc17_dep,
		wayland_client,
		sweetlock_protocols_dep,
		xkbcommon,
		pam,
		cairo,
		pangocairo,
		libjpeg,
		libm,
		threads,
	],
	install: true,
)

install_data(
	'pam/sweetlock',
	install_dir: get_option('sysconfdir') / 'pam.d',
)

# test/ is private and never pushed
if fs.exists('test/meson.build')
	subdir('test')
endif
