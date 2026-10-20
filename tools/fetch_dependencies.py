#!/usr/bin/env python3
"""Download only committed lock entries and verify SHA-256 before use."""
import hashlib
import json
from pathlib import Path
import urllib.request
ROOT=Path(__file__).resolve().parents[1]
lock=json.loads((ROOT/'cmake/dependencies.lock.json').read_text())
downloads=ROOT/'.deps/downloads';downloads.mkdir(parents=True,exist_ok=True)
for name,entry in lock['sources'].items():
    path=downloads/entry['file']
    if not path.exists():
        partial=path.with_suffix('.partial')
        with urllib.request.urlopen(entry['url'],timeout=120) as source,partial.open('wb') as target:
            while block:=source.read(1024*1024):target.write(block)
        assert hashlib.sha256(partial.read_bytes()).hexdigest()==entry['sha256'],f'Checksum mismatch: {name}'
        partial.replace(path)
    assert hashlib.sha256(path.read_bytes()).hexdigest()==entry['sha256'],f'Checksum mismatch: {name}'
    print('Verified',name,entry['version'])
