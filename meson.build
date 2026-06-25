project(
	'locky',
	'c',
	version: '0.1.0',
	license: 'MIT',
	meson_version: '>=0.59.0',
	default_options: [
		'c_std=c11',
		'warning_level=2',
		'werror=true',
	],
)

cc = meson.get_compiler('c')

add_project_arguments('-D_GNU_SOURCE', language: 'c')

wayland_client = dependency('wayland-client', version: '>=1.20.0')
wayland_protos = dependency('wayland-protocols', version: '>=1.25')
wayland_scanner = dependency('wayland-scanner', version: '>=1.15.0', native: true)
cairo = dependency('cairo')
xkbcommon = dependency('xkbcommon')
pam = cc.find_library('pam')

wayland_scanner_prog = find_program(
	wayland_scanner.get_variable('wayland_scanner'),
	native: true,
)

wl_protocol_dir = wayland_protos.get_variable('pkgdatadir')

wayland_scanner_code = generator(
	wayland_scanner_prog,
	output: '@BASENAME@-protocol.c',
	arguments: ['private-code', '@INPUT@', '@OUTPUT@'],
)

wayland_scanner_client = generator(
	wayland_scanner_prog,
	output: '@BASENAME@-client-protocol.h',
	arguments: ['client-header', '@INPUT@', '@OUTPUT@'],
)

# Locking is built on the secure session-lock protocol
client_protocols = [
	wl_protocol_dir / 'staging/ext-session-lock/ext-session-lock-v1.xml',
]

protocols_src = []
foreach xml : client_protocols
	protocols_src += wayland_scanner_code.process(xml)
	protocols_src += wayland_scanner_client.process(xml)
endforeach

locky_inc = include_directories('include')

executable(
	'locky',
	files(
		'src/main.c',
		'src/log.c',
		'src/buffer.c',
		'src/render.c',
		'src/output.c',
		'src/lock.c',
		'src/seat.c',
		'src/password.c',
		'src/unicode.c',
		'src/registry.c',
		'src/auth/auth.c',
		'src/auth/comm.c',
		'src/auth/pam.c',
	),
	protocols_src,
	include_directories: locky_inc,
	dependencies: [wayland_client, cairo, xkbcommon, pam],
	install: true,
)

install_data(
	'pam/locky',
	install_dir: get_option('sysconfdir') / 'pam.d',
)
