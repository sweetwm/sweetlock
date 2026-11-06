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

wayland_client = dependency('wayland-client')
# ext-session-lock-v1 arrived in 1.25
wayland_protocols = dependency('wayland-protocols', version: '>= 1.25')
wayland_scanner = find_program(
	dependency('wayland-scanner', native: true).get_variable('wayland_scanner'),
	native: true,
)

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

executable(
	'sweetlock',
	'src/main.c',
	'src/app.c',
	'src/args.c',
	'src/config/load.c',
	'src/config/parse.c',
	'src/config/schema.c',
	'src/log.c',
	'src/wayland/buffer.c',
	'src/wayland/display.c',
	'src/wayland/lock.c',
	'src/wayland/output.c',
	include_directories: sweetlock_inc,
	dependencies: [tomlc17_dep, wayland_client, sweetlock_protocols_dep],
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
