#!/usr/bin/env python3
"""Check the native ownership boundary and actual desktop/worker link dependencies."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('build', type=Path)
p.add_argument('--output', type=Path)
a = p.parse_args()
ui_sources = [f for folder in ('apps/desktop', 'src/domain', 'src/protocol', 'src/render')
              for f in (ROOT/folder).rglob('*') if f.suffix in ('.hpp', '.cpp')]
for f in ui_sources:
    assert not re.search(r'#include\s*[<"](?:TopoDS|BRep|OpenCASCADE|sqlite|QtTest|QTest|geometry/)', f.read_text()), f
exe = a.build/'cad-desktop'
if sys.platform == 'darwin':
    exe = a.build/'cad-desktop.app/Contents/MacOS/cad-desktop'
    command = ['otool', '-L']
elif sys.platform.startswith('linux'):
    command = ['ldd']
else:
    raise SystemExit('Blocked: Windows binary dependency inspection needs MSVC dumpbin runner')
ui = subprocess.check_output([*command, str(exe)], text=True)
worker = subprocess.check_output([*command, str(a.build/'cad-worker')], text=True)
assert not re.search(r'(?:libTK|Qt6Test|sqlite)', ui), ui
assert 'TKernel' in worker and 'Qt6Test' not in worker, worker
if sys.platform == 'darwin':
    packaged = a.build/'cad-desktop.app/Contents/MacOS/cad-worker'
    assert packaged.read_bytes() == (a.build/'cad-worker').read_bytes(), 'Packaged worker differs'
record = {'status': 'Pass', 'scope': 'Module 1 ownership on this built target only',
          'desktop_dependencies': ui, 'worker_dependencies': worker,
          'ui_source_files_checked': len(ui_sources),
          'desktop_sha256': hashlib.sha256(exe.read_bytes()).hexdigest()}
if a.output:
    a.output.write_text(json.dumps(record, indent=2)+'\n')
print('Pass: UI has no exact-kernel/database/test dependency; worker owns OCCT; packaged worker matches')
