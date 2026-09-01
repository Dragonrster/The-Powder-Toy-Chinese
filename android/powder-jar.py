import os
import os.path
import shutil
import subprocess
import sys

(
	script,
	javac,
	jar,
	source_dir,
	build_dir,
	private_name,
	powder_jar_name,
	android_jar,
	java_runtime,
	sdl_jar,
	debug_release,
	*javac_sources,
) = sys.argv

powder_jar_path = os.path.join(build_dir, powder_jar_name)
private_dir     = os.path.join(build_dir, private_name)
class_dir       = os.path.join(private_dir, 'class')

if os.path.exists(powder_jar_path):
	os.remove(powder_jar_path)
if os.path.exists(class_dir):
	shutil.rmtree(class_dir)
os.makedirs(class_dir)

# Copy every supplied .java into a canonical package layout so javac can
# resolve same-package references (e.g. PowderActivity -> VirtualControls).
pkgroot = os.path.join(private_dir, 'src')
if os.path.exists(pkgroot):
	shutil.rmtree(pkgroot)
os.makedirs(pkgroot)
normalized_sources = []
for source in javac_sources:
	# source is relative to build_dir; resolve it, then place under uk/co/powdertoy/tpt/
	abs_source = source if os.path.isabs(source) else os.path.join(build_dir, source)
	basename = os.path.basename(abs_source)
	dest = os.path.join(pkgroot, 'uk', 'co', 'powdertoy', 'tpt', basename)
	os.makedirs(os.path.dirname(dest), exist_ok = True)
	shutil.copyfile(abs_source, dest)
	normalized_sources.append(dest)

if subprocess.run([
	javac,
	'-d', class_dir,
	'-source', '1.8',
	'-target', '1.8',
	'-bootclasspath', java_runtime,
	'-classpath', os.pathsep.join([ android_jar, sdl_jar ]),
	'-sourcepath', pkgroot,
	*normalized_sources,
], cwd = build_dir).returncode:
	sys.exit(1)

jar_inputs = []
for root, dirs, files in os.walk(class_dir):
	for name in files:
		if name.endswith(".class"):
			jar_inputs.append(os.path.relpath(os.path.join(root, name), start = class_dir))
if subprocess.run([
	jar,
	'cMf',
	powder_jar_path,
	*jar_inputs,
], cwd = class_dir).returncode:
	sys.exit(1)
