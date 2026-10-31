#!/usr/bin/env python3
"""Create a relocatable Mac client beside the existing distribution outputs."""
import argparse
import re
from pathlib import Path
import platform
import plistlib
import shutil
import subprocess
import sys
import tempfile
import struct

if sys.platform != 'darwin':
    raise SystemExit('Mac packaging requires macOS')
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build-dir', type=Path, required=True)
parser.add_argument('--qt-prefix', type=Path, required=True)
parser.add_argument('--dependency-prefix', type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
build = args.build_dir.resolve()
qt_prefix = args.qt_prefix.resolve()
prefix = args.dependency_prefix.resolve()
qmake = qt_prefix / 'bin/qmake'

def run(*args, **kwargs):
    subprocess.run([str(a) for a in args], check=True, **kwargs)

def copy_file(source, destination):
    copied = Path(shutil.copy(source, destination))
    copied.chmod(copied.stat().st_mode | 0o200)
    return copied

def query(key):
    return Path(subprocess.check_output([qmake, '-query', key], text=True).strip())

def dependencies(binary):
    linked = [line.strip().split(' (')[0] for line in subprocess.check_output(
        ['otool', '-L', str(binary)], text=True).splitlines()[1:]]
    identity = subprocess.check_output(['otool', '-D', str(binary)], text=True).splitlines()[1:]
    return [dep for dep in linked if dep not in identity]

def machos():
    result = []
    for path in app.rglob('*'):
        if path.is_file() and not path.is_symlink():
            with path.open('rb') as binary:
                if binary.read(4) in (b'\xcf\xfa\xed\xfe', b'\xce\xfa\xed\xfe', b'\xca\xfe\xba\xbe'):
                    result.append(path)
    return result

source = build / 'bin/AuSceneClient.app'
if not source.exists():
    raise SystemExit('Build the scene client first with make mac-client')
output = build / 'bin'
staging = tempfile.TemporaryDirectory(prefix='soh-package-')
app = Path(staging.name) / 'OpenShores.app'
output.mkdir(exist_ok=True)
shutil.copytree(source, app, symlinks=True, copy_function=shutil.copy)
contents = app / 'Contents'
frameworks = contents / 'Frameworks'
resources = contents / 'Resources'
frameworks.mkdir(exist_ok=True)
resources.mkdir(exist_ok=True)
exe = contents / 'MacOS/OpenShores'
(contents / 'MacOS/AuSceneClient').rename(exe)
with (contents / 'Info.plist').open('rb') as f:
    info = plistlib.load(f)
versions = (root / 'src/lib/util/auversion.h').read_text()
version_parts = [re.search(r'g_' + key + r'\s*=\s*"([^"]+)"', versions)[1]
                 for key in ['Version', 'LoginVersion', 'SceneVersion']]
version = ''.join(version_parts).removeprefix('OS')
info.update(CFBundleExecutable='OpenShores', CFBundleName='OpenShores',
            CFBundleIdentifier='net.openshores.client', CFBundleIconFile='SoH.icns',
            CFBundleShortVersionString=version, CFBundleVersion=version,
            LSMinimumSystemVersion='11.0')
with (contents / 'Info.plist').open('wb') as f:
    plistlib.dump(info, f)
# Use the same artwork referenced by the Windows executable resource.
with tempfile.TemporaryDirectory(prefix='soh-icon-') as temporary:
    iconset = Path(temporary) / 'SoH.iconset'
    iconset.mkdir()
    png = Path(temporary) / 'windows-icon.png'
    run('sips', '-s', 'format', 'png', root / 'src/app/clients/scene/resources/data/SoHIcon.ico', '--out', png)
    chunks = []
    for size, kind in [(16, b'icp4'), (32, b'icp5'), (64, b'icp6'), (128, b'ic07'),
                       (256, b'ic08'), (512, b'ic09'), (1024, b'ic10')]:
        image = iconset / f'icon_{size}.png'
        run('sips', '-z', size, size, png, '--out', image)
        data = image.read_bytes()
        chunks.append(kind + struct.pack('>I', len(data) + 8) + data)
    payload = b''.join(chunks)
    (resources / 'SoH.icns').write_bytes(b'icns' + struct.pack('>I', len(payload) + 8) + payload)
copy_file(build / 'bin/GD', resources)
shutil.copytree(build / 'bin/Blueprints', resources / 'Blueprints', dirs_exist_ok=True)
tex = resources / 'tex'
tex.mkdir(exist_ok=True)
for image in (build / 'bin/tex').glob('*.png'):
    copy_file(image, tex)
for library in (build / 'lib').glob('libAu*.7.dylib'):
    copy_file(library, frameworks / library.name)
# SDL2-compat loads SDL3 dynamically rather than declaring a linked dependency.
sdl2 = prefix / 'opt/sdl2/lib/libSDL2-2.0.0.dylib'
copy_file(sdl2, frameworks / sdl2.name)
if b'libSDL3' in sdl2.read_bytes():
    sdl3 = prefix / 'opt/sdl3/lib/libSDL3.dylib'
    copy_file(sdl3, frameworks / sdl3.name)
for binary in [exe, *frameworks.glob('*.dylib')]:
    for dep in dependencies(binary):
        name = Path(dep).name
        if (frameworks / name).exists():
            run('install_name_tool', '-change', dep, '@rpath/' + name, binary)
    if binary != exe:
        run('install_name_tool', '-id', '@rpath/' + binary.name, binary)
copy_file(build / 'bin/sfx.har', resources / 'sfx.har')
(contents / 'MacOS/tex').symlink_to('../Resources/tex')
(contents / 'MacOS/sfx.har').symlink_to('../Resources/sfx.har')
run(query('QT_INSTALL_BINS') / 'macdeployqt', app, '-always-overwrite', '-appstore-compliant')
# macdeployqt can omit dependencies of non-Qt libraries. Close that graph too.
while True:
    added = False
    for binary in machos():
        for dep in dependencies(binary):
            if dep.startswith(('/System/Library/', '/usr/lib/')):
                continue
            name = Path(dep).name
            if dep.startswith('@rpath/') and (frameworks / dep[7:]).exists():
                continue
            if dep.startswith(('@loader_path/', '@executable_path/')):
                continue
            installed = Path(dep) if dep.startswith('/') else prefix / 'lib' / name
            destination = frameworks / name
            if not destination.exists():
                if not installed.exists():
                    raise SystemExit(f'Cannot deploy dependency: {dep}')
                copy_file(installed, destination)
                run('install_name_tool', '-id', '@rpath/' + name, destination)
                added = True
            run('install_name_tool', '-change', dep, '@rpath/' + name, binary)
    if not added:
        break
# Remove development search paths so dyld can only use bundled dependencies.
minimum_os = (11, 0)
for binary in machos():
    load = subprocess.check_output(['otool', '-l', str(binary)], text=True).splitlines()
    command = None
    for entry in load:
        if entry.strip().startswith('cmd '):
            command = entry.strip().split()[1]
        field = {'LC_BUILD_VERSION': 'minos', 'LC_VERSION_MIN_MACOSX': 'version'}.get(command)
        if field:
            match = re.match(r'\s*' + field + r'\s+(\d+)\.(\d+)', entry)
            if match:
                minimum_os = max(minimum_os, tuple(map(int, match.groups())))
    for i, line in enumerate(load):
        if line.strip() == 'cmd LC_RPATH':
            path = load[i + 2].strip().split('path ', 1)[1].split(' (')[0]
            if path.startswith('/'):
                run('install_name_tool', '-delete_rpath', path, binary)
# CMake's development rpaths do not necessarily include the bundle directory.
load_commands = subprocess.check_output(['otool', '-l', str(exe)], text=True)
if not re.search(r'^\s*path @executable_path/\.\./Frameworks ', load_commands, re.M):
    run('install_name_tool', '-add_rpath', '@executable_path/../Frameworks', exe)
# Reject undeployed link dependencies, including transitive Qt/plugin dependencies.
for binary in machos():
    for dep in dependencies(binary):
        if dep.startswith(('/System/Library/', '/usr/lib/')):
            continue
        if dep.startswith('@rpath/'):
            resolved = frameworks / dep[len('@rpath/'):]
        elif dep.startswith('@loader_path/'):
            resolved = binary.parent / dep[len('@loader_path/'):]
        elif dep.startswith('@executable_path/'):
            resolved = exe.parent / dep[len('@executable_path/'):]
        else:
            raise SystemExit(f'External dependency in {binary}: {dep}')
        if not resolved.exists():
            raise SystemExit(f'Missing bundled dependency in {binary}: {dep}')
info['LSMinimumSystemVersion'] = '.'.join(map(str, minimum_os))
with (contents / 'Info.plist').open('wb') as f:
    plistlib.dump(info, f)
licenses = resources / 'Licenses'
licenses.mkdir(exist_ok=True)
copy_file(root / 'LICENSE', licenses / 'Hazeron-LICENSE')
origins = {'Qt': query('QT_INSTALL_PREFIX'), 'SDL2': prefix / 'opt/sdl2',
           'SDL3': prefix / 'opt/sdl3'}
for library in frameworks.glob('*.dylib'):
    installed = prefix / 'lib' / library.name
    if installed.exists():
        resolved = installed.resolve()
        if 'Cellar' in resolved.parts:
            index = resolved.parts.index('Cellar')
            origins[resolved.parts[index + 1]] = Path(*resolved.parts[:index + 3])
for name, directory in origins.items():
    for license_file in list(directory.glob('LICENSE*')) + list(directory.glob('COPYING*')):
        if license_file.is_file():
            copy_file(license_file, licenses / (name + '-' + license_file.name))
for path in app.rglob('*'):
    if not path.is_symlink():
        path.chmod(path.stat().st_mode | 0o200)
run('xattr', '-cr', app)
for binary in machos():
    if binary != exe:
        run('codesign', '--force', '--sign', '-', binary)
for framework in frameworks.glob('*.framework'):
    run('codesign', '--force', '--sign', '-', framework)
run('codesign', '--force', '--sign', '-', app)
run('codesign', '--verify', '--deep', '--strict', app)
packages = root / 'build/packages'
packages.mkdir(parents=True, exist_ok=True)
image = packages / ('soh-macos-' + platform.machine() + '.dmg')
run(sys.executable, root / 'tools/macos/dmg.py', '--app', app, '--output', image)
# Remove obsolete Mac ZIP outputs only after a replacement image is verified.
for suffix in ('.zip', '.zip.sha256'):
    (packages / ('soh-macos-' + platform.machine() + suffix)).unlink(missing_ok=True)
destination = output / 'OpenShores.app'
if destination.exists():
    shutil.rmtree(destination)
shutil.copytree(app, destination, symlinks=True, copy_function=shutil.copy)
run('xattr', '-cr', destination)
run('codesign', '--verify', '--deep', '--strict', destination)
staging.cleanup()
print('Packaged:', destination, '\nDisk image:', image, '\nMinimum macOS:', info['LSMinimumSystemVersion'])
