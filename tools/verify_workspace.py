#!/usr/bin/env python3
"""Verify Module 3 input repairs and authorized Module 4.0 workspace; preserve historical evidence."""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--native', action='store_true')
a = p.parse_args()
if sys.platform != 'darwin':
    raise SystemExit('Blocked: this runner requires the available Mac; native Windows/Linux runners are unavailable')
base = ROOT/'docs/testing/artifacts/module-4'
folder = base/'runs'/datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ')
folder.mkdir(parents=True)
record = {'scope': 'Available-Mac Module 3 input/direct-editing repairs and Module 4.0a-c workspace only; stop before 4.1a exact features',
          'status': 'In progress', 'native_requested': a.native,
          'artifacts': str(folder.relative_to(ROOT)), 'steps': [],
          'blocked': ['Windows/Linux native build/UI/filesystem matrix', 'Intel/minimum macOS/additional GPUs',
                      'installed packages/signing', 'OS-network-disabled workflow', 'full physical 8 GB B0–B5 resource budgets', 'physical trackpad input and native assistive technology'],
          'host': {'platform': platform.platform(), 'machine': platform.machine()}}
def save():
    (folder/'verification.json').write_text(json.dumps(record, indent=2)+'\n')
    (base/'verification.json').write_text(json.dumps(record, indent=2)+'\n')
def run(name, command, env=None, timeout=600):
    print('Running', name, flush=True)
    start = time.monotonic()
    with (folder/(name+'.txt')).open('w') as log:
        try:
            r = subprocess.run(command, cwd=ROOT, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=timeout)
            code = r.returncode
        except subprocess.TimeoutExpired:
            code = 124
            log.write('\nCommand timed out\n')
    record['steps'].append({'name': name, 'command': command, 'exit_code': code, 'seconds': time.monotonic()-start})
    if code:
        record['status'] = 'Fail'
    save()
    if code:
        raise SystemExit(f'Fail: {name}; see {folder/name}')
try:
    record['host']['physical_ram_bytes'] = int(subprocess.check_output(['sysctl', '-n', 'hw.memsize'], text=True))
    record['host']['cpu'] = subprocess.check_output(['sysctl', '-n', 'machdep.cpu.brand_string'], text=True).strip()
except (subprocess.CalledProcessError, ValueError):
    record['host']['hardware_check'] = 'Blocked: host inventory denied'
save()
run('configure', ['cmake', '--preset', 'macos-debug'])
run('build', ['cmake', '--build', '--preset', 'macos-debug'])
run('ctest', ['ctest', '--preset', 'macos-debug', '--output-on-failure'], timeout=300)
run('format', [sys.executable, 'tools/format_sources.py'])
run('static-analysis', [sys.executable, '-u', 'tools/static_analysis.py', 'build/macos-debug/compile_commands.json'])
run('ownership', [sys.executable, 'tools/check_architecture.py', 'build/macos-debug', '--output', str(folder/'architecture.json')])
run('research-isolation', [sys.executable, 'tools/check_isolation.py'])
if a.native:
    env = os.environ.copy()
    env.pop('QT_QPA_PLATFORM', None)
    env.update(CAD_NATIVE_TESTS='1')
    for percent, factor in [(100, '.5'), (125, '.625'), (150, '.75'), (175, '.875'), (200, None)]:
        native = folder/f'native-{percent}'
        native.mkdir()
        env['CAD_ARTIFACT_DIR'] = str(native)
        if factor:
            env['QT_SCALE_FACTOR'] = factor
        else:
            env.pop('QT_SCALE_FACTOR', None)
        run(f'native-module3-{percent}', [str(ROOT/'build/macos-debug/module3_contracts'), 'nativeSketches'], env, 90)
        run(f'native-workspace-{percent}', [str(ROOT/'build/macos-debug/module4_workspace'), 'nativeWorkspace'], env, 90)
    env.pop('QT_SCALE_FACTOR', None)
    env['CAD_ARTIFACT_DIR'] = str(folder)
    run('native-module2-regression', [str(ROOT/'build/macos-debug/module2_contracts'), 'nativeDocuments'], env, 90)
    run('native-module1-regression', [str(ROOT/'build/macos-debug/module1_contracts'), 'nativeInteraction'], env, 90)
files = [ROOT/'CMakeLists.txt', ROOT/'CMakePresets.json', ROOT/'.clang-format']
for sub in ('apps', 'src', 'tests', 'tools', 'cmake'):
    files += [f for f in (ROOT/sub).rglob('*') if f.is_file() and f.suffix in ('.cpp', '.hpp', '.h', '.py', '.cmake', '.json', '.patch')]
hashes = {str(f.relative_to(ROOT)): hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted(set(files))}
commit = subprocess.run(['git', 'rev-parse', '--verify', 'HEAD'], cwd=ROOT, text=True, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
identity = {'git_commit': commit.stdout.strip() if commit.returncode == 0 else 'None: uncommitted initial repository',
            'branch': subprocess.check_output(['git', 'symbolic-ref', '--short', 'HEAD'], cwd=ROOT, text=True).strip(),
            'inputs': hashes, 'sha256': hashlib.sha256(json.dumps(hashes, sort_keys=True).encode()).hexdigest(),
            'fixture_B1_sha256': hashlib.sha256((ROOT/'tests/fixtures/B1.json').read_bytes()).hexdigest()}
(folder/'source-identity.json').write_text(json.dumps(identity, indent=2)+'\n')
record['status'] = 'Pass: requested available-Mac checks'
save()
print('Pass: requested input/workspace checks; physical-device/platform/full resource gates remain explicit', flush=True)
