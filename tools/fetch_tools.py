#!/usr/bin/env python3
"""Provision the content-locked formatter wheel into .deps/tooling only."""
import hashlib
import json
from pathlib import Path
import platform
import subprocess
import sys
import urllib.request
ROOT=Path(__file__).resolve().parents[1]
entry=json.loads((ROOT/'cmake/tools.lock.json').read_text())['clang_format']
if platform.system()!='Darwin' or platform.machine()!='arm64':
    raise SystemExit('Blocked: native formatter wheel pin required for this platform; see tools.lock.json')
path=ROOT/'.deps/downloads'/entry['file'];path.parent.mkdir(parents=True,exist_ok=True)
if not path.exists():
    with urllib.request.urlopen('https://pypi.org/pypi/clang-format/21.1.8/json',timeout=30) as response:
        release=json.load(response)
    file=next(item for item in release['urls'] if item['filename']==entry['file'])
    assert file['digests']['sha256']==entry['sha256'],'Registry digest differs from committed pin'
    with urllib.request.urlopen(file['url'],timeout=60) as source,path.with_suffix('.partial').open('wb') as out:
        while chunk:=source.read(1024*1024):out.write(chunk)
    path.with_suffix('.partial').replace(path)
assert hashlib.sha256(path.read_bytes()).hexdigest()==entry['sha256'],'Formatter checksum mismatch'
subprocess.run([sys.executable,'-m','pip','install','--upgrade','--no-deps','--no-index','--target',str(ROOT/'.deps/tooling'),str(path)],check=True)
