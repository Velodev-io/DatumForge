#!/usr/bin/env python3
"""Pinned project-local clang-format plus encoding/whitespace checks."""
import argparse
from pathlib import Path
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--write',action='store_true');args=p.parse_args()
exe=ROOT/'.deps/tooling/clang_format/data/bin/clang-format'
if sys.platform=='win32':exe=exe.with_suffix('.exe')
if not exe.is_file():raise SystemExit('Blocked: provision clang-format 21.1.8 from cmake/tools.lock.json for this platform')
version=subprocess.check_output([str(exe),'--version'],text=True)
assert '21.1.8' in version,version
files=[str(path) for folder in ('tests','src','apps') for path in sorted((ROOT/folder).rglob('*')) if path.suffix in {'.cpp','.h','.hpp'}]
subprocess.run([str(exe),*(['-i'] if args.write else ['--dry-run','--Werror']),*files],check=True)
subprocess.run([sys.executable,str(ROOT/'tools/check_source.py')],check=True)
print('Pass: pinned clang-format 21.1.8'+(' applied' if args.write else ' check'))
