#!/usr/bin/env python3
"""Verify protected research and reject tracked secrets/user model payloads."""
import hashlib
import json
from pathlib import Path
import subprocess
ROOT=Path(__file__).resolve().parents[1]
expected=json.loads((ROOT/'docs/testing/artifacts/module-0/research-baseline.json').read_text())
actual={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest()
        for p in sorted((ROOT/'research').rglob('*')) if p.is_file()}
assert actual==expected,'Research changed'
tracked=subprocess.check_output(['git','ls-files','--cached','--others','--exclude-standard','-z'],cwd=ROOT).decode().split('\0')
for name in filter(None,tracked):
    if name.startswith('research/'):continue
    p=Path(name)
    assert p.suffix.lower() not in {'.pem','.p12','.pfx','.sqlite','.db','.cadproj','.step','.iges','.brep'},name
    assert not name.startswith(('.deps/','build/','.local/')),name
allowed = {'https://github.com/Velodev-io/DatumForge.git', 'git@github.com:Velodev-io/DatumForge.git'}
remotes = subprocess.run(['git', 'config', '--get-regexp', r'^remote\..*\.(url|pushurl)$'],
                         cwd=ROOT, text=True, capture_output=True)
assert remotes.returncode in (0, 1), 'Cannot inspect configured remotes'
for line in remotes.stdout.splitlines():
    key, url = line.split(None, 1)
    assert key in {'remote.origin.url', 'remote.origin.pushurl'} and url in allowed, 'Unexpected remote destination'
print(f'Pass: {len(expected)} research files unchanged; {len(list(filter(None,tracked)))} tracked/pending paths inspected; remote destinations checked')
