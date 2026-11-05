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

executable(
	'sweetlock',
	'src/main.c',
	'src/log.c',
	include_directories: include_directories('src'),
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
