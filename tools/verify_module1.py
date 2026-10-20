#!/usr/bin/env python3
"""Reproduce Module 1 build, contracts, ownership and optional real native Mac checks.

Run from a logged-in desktop terminal. Native scale overrides exercise Qt's
coordinate mapping on one Retina display; they do not certify other monitors/OSes.
"""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--native', action='store_true')
a = p.parse_args()
if sys.platform != 'darwin':
    raise SystemExit('Blocked: this evidence runner is validated on macOS only; use candidate native presets and port the runner on Windows/Linux')
artifact = ROOT/'docs/testing/artifacts/module-1'
artifact.mkdir(parents=True, exist_ok=True)
steps = []

def run(name, command, env=None, timeout=600):
    print('Running', name, flush=True)
    start = time.monotonic()
    with (artifact/(name+'.txt')).open('w') as log:
        try:
            result = subprocess.run(command, cwd=ROOT, env=env, stdout=log,
                                    stderr=subprocess.STDOUT, timeout=timeout)
            code = result.returncode
        except subprocess.TimeoutExpired:
            code = 124
            log.write('\nFail: command timed out\n')
    steps.append({'name': name, 'command': command, 'exit_code': code,
                  'seconds': time.monotonic()-start})
    (artifact/'verification.json').write_text(json.dumps({
        'recorded_at_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
        'scope': 'Module 1 reference workspace; not full R1 acceptance',
        'steps': steps, 'native_requested': a.native,
        'status': 'Blocked' if code==77 else 'Fail' if code else 'In progress',
        'Windows': 'Blocked: host unavailable',
        'Linux_X11_Wayland': 'Blocked: hosts unavailable'}, indent=2)+'\n')
    if code:
        raise SystemExit(f'{"Blocked" if code==77 else "Fail"}: {name} exit {code}; see {artifact/(name+".txt")}')

run('configure', ['cmake', '--preset', 'macos-debug'])
run('build', ['cmake', '--build', '--preset', 'macos-debug'])
run('ctest', ['ctest', '--preset', 'macos-debug', '--output-on-failure'], timeout=120)
run('format', [sys.executable, 'tools/format_sources.py'])
run('static-analysis', [sys.executable, '-u', 'tools/static_analysis.py', 'build/macos-debug/compile_commands.json'])
run('research-isolation', [sys.executable, 'tools/check_isolation.py'])
run('ownership', [sys.executable, 'tools/check_architecture.py', 'build/macos-debug', '--output', str(artifact/'architecture.json')])
if a.native:
    matrix = []
    for percent, factor in [(100, '.5'), (125, '.625'), (150, '.75'), (175, '.875'), (200, None)]:
        env = os.environ.copy()
        env.update(CAD_NATIVE_TESTS='1', CAD_ARTIFACT_DIR=str(artifact))
        if factor is None:
            env.pop('QT_SCALE_FACTOR', None)
        else:
            env['QT_SCALE_FACTOR'] = factor
        command = [str(ROOT/'build/macos-debug/module1_contracts'), 'nativeInteraction', '-o',
                   str(artifact/f'native-scale{percent}.txt')+',txt']
        run(f'native-run-{percent}', command, env, timeout=30)
        matrix.append({'effective_percent': percent, 'qt_scale_factor': factor,
                       'physical_display': 'Mac Retina native DPR 2', 'exit_code': 0})
    (artifact/'native-matrix.json').write_text(json.dumps(matrix, indent=2)+'\n')
if a.native:
    run('measurement-summary', [sys.executable, 'tools/summarize_module1.py'])
files = [ROOT/'CMakeLists.txt', ROOT/'CMakePresets.json', ROOT/'.clang-format']
for folder in ('apps', 'src', 'tests', 'tools', 'cmake'):
    files.extend(f for f in (ROOT/folder).rglob('*') if f.is_file() and f.suffix in
                 ('.cpp', '.hpp', '.h', '.py', '.cmake', '.json', '.patch'))
hashes = {str(f.relative_to(ROOT)): hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted(set(files))}
commit = subprocess.run(['git', 'rev-parse', '--verify', 'HEAD'], cwd=ROOT, text=True, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
identity = {'git_commit': commit.stdout.strip() if commit.returncode == 0 else 'None: repository has no initial commit', 'inputs': hashes,
            'sha256': hashlib.sha256(json.dumps(hashes, sort_keys=True).encode()).hexdigest()}
(artifact/'source-identity.json').write_text(json.dumps(identity, indent=2)+'\n')
record=json.loads((artifact/'verification.json').read_text());record['status']='Pass: requested available-host checks';(artifact/'verification.json').write_text(json.dumps(record,indent=2)+'\n')
print('Pass: requested available-host checks; full platform/release gates remain Blocked', flush=True)
