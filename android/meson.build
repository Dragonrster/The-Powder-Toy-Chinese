# The shared conf_data has already been consumed by configure_file calls in
# other subdirectories and can no longer be modified, so build a local copy
# for the keys the android templates need.
android_conf_data = configuration_data()
android_conf_data.merge_from(conf_data)

# The PowderActivity template embeds the VirtualControls call through this
# placeholder, so it must be set before configuring the template below.
virtual_controls_enabled = get_option('android_virtual_controls')
if virtual_controls_enabled
	android_conf_data.set('VIRTUAL_CONTROLS', 'VirtualControls.attach(this);')
	virtual_controls_source = configure_file(
		input: 'VirtualControls.template.java',
		output: 'VirtualControls.java',
		configuration: android_conf_data,
	)
else
	android_conf_data.set('VIRTUAL_CONTROLS', '')
endif

android_manifest_xml = configure_file(
	input: 'AndroidManifest.template.xml',
	output: 'AndroidManifest.xml',
	configuration: android_conf_data,
)
powder_jar_sources = configure_file(
	input: 'PowderActivity.template.java',
	output: 'PowderActivity.java',
	configuration: android_conf_data,
)

subdir('res')

android_platform_jar = meson.get_external_property('android_platform_jar')
java_runtime_jar = meson.get_external_property('java_runtime_jar')
android_keystore = get_option('android_keystore')
sdl_jar = tpt_libs.get_variable('sdl_jar')
powder_jar_command = [
	python3_prog,
	files('powder-jar.py'),
	find_program('javac'),
	find_program('jar'),
	meson.current_source_dir(),
	meson.project_build_root(),
	'@PRIVATE_DIR@',
	'@OUTPUT@',
	android_platform_jar,
	java_runtime_jar,
	sdl_jar,
	is_debug ? 'debug' : 'release',
	powder_jar_sources,
]
if virtual_controls_enabled
	powder_jar_command += virtual_controls_source
endif
powder_jar = custom_target(
	'powder-jar',
	output: app_exe + '.jar',
	command: powder_jar_command,
)
unaligned_apk = custom_target(
	'build-apk',
	output: app_exe + '.unaligned.apk',
	input: powder_sha,
	command: [
		python3_prog,
		files('build-apk.py'),
		find_program('d8'),
		find_program('aapt'),
		find_program('aapt2'),
		meson.current_source_dir(),
		meson.project_build_root(),
		'@PRIVATE_DIR@',
		'@OUTPUT@',
		'@INPUT@',
		android_platform_jar,
		sdl_jar,
		powder_jar,
		host_arch,
		is_debug ? 'debug' : 'release',
		android_manifest_xml,
		android_resources,
	],
)
unsigned_apk = custom_target(
	'align-apk',
	output: app_exe + '.unsigned.apk',
	input: unaligned_apk,
	command: [
		python3_prog,
		files('align-apk.py'),
		find_program('zipalign'),
		meson.project_build_root(),
		'@OUTPUT@',
		'@INPUT@',
	],
)
signed_apk = custom_target(
	'sign-apk',
	output: app_exe + '.apk',
	input: unsigned_apk,
	command: [
		python3_prog,
		files('sign-apk.py'),
		find_program('apksigner'),
		meson.project_build_root(),
		'@OUTPUT@',
		'@INPUT@',
		android_keystore,
		get_option('android_keyalias'),
	],
)
adb = find_program('adb', required: false)
if adb.found()
	install_apk = custom_target(
		'install-apk',
		input: signed_apk,
		output: 'install-apk',
		command: [ python3_prog, files('install-apk.py'), adb, meson.project_build_root(), '@OUTPUT@', '@INPUT@' ],
	)
	custom_target(
		'run-apk',
		input: [ signed_apk, install_apk ],
		build_always_stale: true,
		output: 'run-apk',
		command: [ python3_prog, files('run-apk.py'), adb, meson.project_build_root(), '@OUTPUT@', '@INPUT0@', app_id ],
	)
endif
