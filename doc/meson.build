scdoc = find_program('scdoc', required: false)

if scdoc.found()
	foreach section : ['1', '5']
		page = 'sweetlock.' + section
		custom_target(
			page,
			input: page + '.scd',
			output: page,
			command: scdoc,
			feed: true,
			capture: true,
			install: true,
			install_dir: get_option('mandir') / ('man' + section),
		)
	endforeach
endif
