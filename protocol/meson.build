wl_protocol_dir = wayland_protocols.get_variable('pkgdatadir')

protocols = [
	wl_protocol_dir / 'stable' / 'viewporter' / 'viewporter.xml',
	wl_protocol_dir / 'staging' / 'ext-session-lock' / 'ext-session-lock-v1.xml',
	wl_protocol_dir / 'staging' / 'fractional-scale' / 'fractional-scale-v1.xml',
]

protocol_src = []
protocol_headers = []

foreach xml : protocols
	base = fs.stem(xml)

	protocol_headers += custom_target(
		base + '-client-protocol.h',
		input: xml,
		output: base + '-client-protocol.h',
		command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
	)

	protocol_src += custom_target(
		base + '-protocol.c',
		input: xml,
		output: base + '-protocol.c',
		command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
	)
endforeach

sweetlock_protocols = static_library(
	'sweetlock-protocols',
	protocol_src,
	protocol_headers,
	dependencies: wayland_client,
)

sweetlock_protocols_dep = declare_dependency(
	link_with: sweetlock_protocols,
	# listing the headers as sources puts their build dir on the include path
	sources: protocol_headers,
)
