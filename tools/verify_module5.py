#!/usr/bin/env python3
"""Verify Module 5 dependency, history and reference workflows on the available Mac; preserve historical runs."""
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
p.add_argument('--clean', action='store_true')
a = p.parse_args()
if sys.platform != 'darwin':
    raise SystemExit('Blocked: this evidence runner targets the available Mac; native Windows/Linux runners unavailable')
base = ROOT/'docs/testing/artifacts/module-5'
folder = base/'runs'/datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ')
folder.mkdir(parents=True)
record = {'scope': 'Module 5 dependency/history/reference/cache and frozen B2/native regressions on available Mac; stop before Module 6',
          'status': 'In progress', 'native_requested': a.native, 'clean_requested': a.clean,
          'artifacts': str(folder.relative_to(ROOT)), 'steps': [],
          'blocked': ['Physical mouse/trackpad ergonomics', 'Windows/Linux/X11/Wayland native builds',
                      'Intel/minimum macOS/additional GPUs', 'Native assistive technology/alternate appearance',
                      'Installed builds/signing/upgrades', 'Controlled OS-network-disabled workflow',
                      'Full physical 8 GB B0–B5 resource budgets', 'Physical power loss/full-volume storage'],
          'host': {'platform': platform.platform(), 'machine': platform.machine()}}
def save():
    content = json.dumps(record, indent=2)+'\n'
    (folder/'verification.json').write_text(content)
    (base/'verification.json').write_text(content)
def run(name, command, env=None, timeout=600):
    print('Running', name, flush=True)
    start = time.monotonic()
    with (folder/(name+'.txt')).open('w') as log:
        try:
            result = subprocess.run(command, cwd=ROOT, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=timeout)
            code = result.returncode
        except subprocess.TimeoutExpired:
            code = 124
            log.write('\nCommand timed out\n')
    record['steps'].append({'name': name, 'command': command, 'exit_code': code, 'seconds': time.monotonic()-start})
    if code:
        record['status'] = 'Fail'
    save()
    if code:
        raise SystemExit(f'Fail: {name}; see {folder}')
record['host']['physical_ram_bytes'] = int(subprocess.check_output(['sysctl', '-n', 'hw.memsize'], text=True))
record['host']['cpu'] = subprocess.check_output(['sysctl', '-n', 'machdep.cpu.brand_string'], text=True).strip()
record['host']['os_version'] = subprocess.check_output(['sw_vers'], text=True).strip()
save()
def inputs():
    paths = [ROOT/'CMakeLists.txt', ROOT/'CMakePresets.json', ROOT/'.clang-format']
    for sub in ('apps','src','tests','tools','cmake'):
        paths += [f for f in (ROOT/sub).rglob('*') if f.is_file() and f.suffix in ('.cpp','.hpp','.h','.py','.cmake','.json','.patch')]
    return {str(f.relative_to(ROOT)):hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted(set(paths))}
frozen = inputs()
run('configure', ['cmake', '--preset', 'macos-debug'])
run('build', ['cmake', '--build', '--preset', 'macos-debug'])
if inputs()!=frozen:
    record['status']='Fail: source changed during build'
    save()
    raise SystemExit(record['status'])
run('ctest', ['ctest', '--preset', 'macos-debug', '--output-on-failure'], timeout=2400)
run('format', [sys.executable, 'tools/format_sources.py'])
run('static-analysis', [sys.executable, '-u', 'tools/static_analysis.py', 'build/macos-debug/compile_commands.json'])
if (ROOT/'build/macos-debug/module5-b2.json').is_file():
    import shutil
    shutil.copy2(ROOT/'build/macos-debug/module5-b2.json', folder/'b2-correctness.json')
run('ownership', [sys.executable, 'tools/check_architecture.py', 'build/macos-debug', '--output', str(folder/'architecture.json')])
run('research-isolation', [sys.executable, 'tools/check_isolation.py'])
if a.native:
    env = os.environ.copy()
    env.pop('QT_QPA_PLATFORM', None)
    env['CAD_NATIVE_TESTS'] = '1'
    for percent, factor in [(100, '.5'), (125, '.625'), (150, '.75'), (175, '.875'), (200, None)]:
        native = folder/f'native-{percent}'
        native.mkdir()
        env['CAD_ARTIFACT_DIR'] = str(native)
        if factor:
            env['QT_SCALE_FACTOR'] = factor
        else:
            env.pop('QT_SCALE_FACTOR', None)
        run(f'native-history-{percent}', [str(ROOT/'build/macos-debug/module5_contracts'), 'nativeHistory'], env, 180)
        run(f'native-features-{percent}', [str(ROOT/'build/macos-debug/module4_contracts'), 'nativeFeatures'], env, 180)
        run(f'native-workspace-{percent}', [str(ROOT/'build/macos-debug/module4_workspace'), 'nativeWorkspace'], env, 90)
        run(f'native-module3-{percent}', [str(ROOT/'build/macos-debug/module3_contracts'), 'nativeSketches'], env, 90)
    env.pop('QT_SCALE_FACTOR', None)
    env['CAD_ARTIFACT_DIR'] = str(folder)
    run('native-module1-regression', [str(ROOT/'build/macos-debug/module1_contracts'), 'nativeInteraction'], env, 90)
    run('native-module2-regression', [str(ROOT/'build/macos-debug/module2_contracts'), 'nativeDocuments'], env, 90)
if a.clean:
    run('clean-reproduction', [sys.executable, 'tools/clean_reproduce.py', '--module', '5', '--output', str(folder/'clean-reproduction-details.txt')], timeout=2400)
files = [ROOT/'CMakeLists.txt', ROOT/'CMakePresets.json', ROOT/'.clang-format']
for sub in ('apps', 'src', 'tests', 'tools', 'cmake'):
    files += [f for f in (ROOT/sub).rglob('*') if f.is_file() and f.suffix in ('.cpp', '.hpp', '.h', '.py', '.cmake', '.json', '.patch')]
hashes = {str(f.relative_to(ROOT)): hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted(set(files))}
if hashes!=frozen:
    record['status']='Fail: source changed during verification'
    save()
    raise SystemExit(record['status'])
identity = {'git_commit': subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
            'dirty_paths': subprocess.check_output(['git','status','--short'],cwd=ROOT,text=True).splitlines(),
            'branch': subprocess.check_output(['git', 'symbolic-ref', '--short', 'HEAD'], cwd=ROOT, text=True).strip(),
            'inputs': hashes, 'sha256': hashlib.sha256(json.dumps(hashes, sort_keys=True).encode()).hexdigest(),
            'fixture_B1_sha256': hashlib.sha256((ROOT/'tests/fixtures/B1.json').read_bytes()).hexdigest()}
(folder/'source-identity.json').write_text(json.dumps(identity, indent=2)+'\n')
record['status'] = 'Pass: requested available-Mac checks; full module platform/device acceptance Blocked'
save()
print('Pass: requested Module 5 available-Mac checks; stop before Module 6', flush=True)
