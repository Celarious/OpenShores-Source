#!/usr/bin/env python3
"""Create a compressed drag-and-drop disk image from a signed Mac app."""
import argparse
import hashlib
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

if sys.platform != 'darwin':
    raise SystemExit('DMG creation requires macOS')

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--app', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
app = args.app.resolve()
output = args.output.resolve()
if not app.is_dir() or app.suffix != '.app':
    raise SystemExit('Expected a packaged .app; run make mac-client first')
if output.suffix != '.dmg':
    raise SystemExit('Output must have a .dmg extension')

def run(*command):
    subprocess.run([str(arg) for arg in command], check=True)

output.parent.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix='openshores-dmg-', dir=output.parent) as temporary:
    temporary = Path(temporary)
    contents = temporary / 'contents'
    contents.mkdir()
    staged_app = contents / app.name
    shutil.copytree(app, staged_app, symlinks=True, copy_function=shutil.copy)
    (contents / 'Applications').symlink_to('/Applications', target_is_directory=True)
    # Finder can add metadata after an app is launched. Scrub the staging copy
    # before validating its original signature; never re-sign modified content.
    run('xattr', '-cr', staged_app)
    run('codesign', '--verify', '--deep', '--strict', staged_app)
    # HFS+ image creation can add Finder bundle flags after the staging check.
    # Scrub the actual volume before compressing it, then verify the final image.
    writable_image = temporary / 'writable.dmg'
    mount = temporary / 'mounted'
    mount.mkdir()

    def verify_image(path, scrub=False):
        options = [] if scrub else ['-readonly']
        run('hdiutil', 'attach', '-nobrowse', *options, '-mountpoint', mount, path)
        try:
            mounted_app = mount / app.name
            if scrub:
                run('xattr', '-cr', mounted_app)
            run('codesign', '--verify', '--deep', '--strict', mounted_app)
        finally:
            run('hdiutil', 'detach', mount)

    run('hdiutil', 'create', '-volname', 'OpenShores', '-fs', 'HFS+',
        '-format', 'UDRW', '-srcfolder', contents, writable_image)
    verify_image(writable_image, scrub=True)
    image = temporary / 'image.dmg'
    run('hdiutil', 'convert', writable_image, '-format', 'UDZO', '-o', image)
    run('hdiutil', 'verify', image)
    verify_image(image)
    digest = hashlib.sha256()
    with image.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    checksum = temporary / 'checksum.sha256'
    checksum.write_text(digest.hexdigest() + '  ' + output.name + '\n')
    image.replace(output)
    checksum.replace(output.with_suffix('.dmg.sha256'))
print('Disk image:', output)
