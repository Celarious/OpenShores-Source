#!/usr/bin/env python3
"""Generate the shared runtime audio archive with the native hashpack tool."""
import argparse
import hashlib
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--hashpack', type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
source = root / 'resources/audio/source'
archive = root / 'build/assets/sfx.har'
stamp = archive.with_name('sfx.macos.inputs.sha256')
tool = args.hashpack.resolve()
digest = hashlib.sha256(tool.read_bytes())
for path in sorted(source.glob('*.wav')):
    digest.update(path.name.encode())
    digest.update(path.read_bytes())
fingerprint = digest.hexdigest()
if archive.exists() and stamp.exists() and stamp.read_text() == fingerprint:
    print('Runtime audio is up to date.')
else:
    archive.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=archive.parent) as temporary:
        generated = Path(temporary).resolve() / 'sfx.har'
        subprocess.run([str(tool), '-c', str(generated), '*.wav'], cwd=source, check=True)
        if not generated.stat().st_size:
            raise SystemExit('hashpack generated an empty sound archive')
        generated.replace(archive)
    stamp.write_text(fingerprint)
