#!/usr/bin/env python3
"""Record CM3 development-build evidence; never infer physical/platform acceptance."""
import datetime
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import time

ROOT = Path.cwd().resolve()
FOLDER = ROOT / 'docs/testing/artifacts/core-modeling/cm3-completion-20261031/verification' / datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ')
FOLDER.mkdir(parents=True)
print(FOLDER, flush=True)

def inputs():
    paths = [ROOT / 'CMakeLists.txt', ROOT / 'CMakePresets.json', ROOT / '.clang-format', ROOT / 'docs/scripts/check_specs.py']
    for top in ['apps', 'src', 'tests', 'tools', 'cmake']:
        paths += [p for p in (ROOT / top).rglob('*') if p.is_file() and p.suffix in {'.cpp', '.hpp', '.h', '.py', '.swift', '.cmake', '.json', '.patch'}]
    return {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(set(paths))}

frozen = inputs()
identity = {'branch': subprocess.check_output(['git', 'branch', '--show-current'], cwd=ROOT, text=True).strip(),
            'head': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
            'sha256': hashlib.sha256(json.dumps(frozen, sort_keys=True).encode()).hexdigest(), 'inputs': frozen}
(FOLDER / 'source-identity.json').write_text(json.dumps(identity, indent=2) + '\n')
record = {'status': 'In progress', 'scope': 'CM3 schema-8 dimensions, true paths, ordinates, constrained drag, confirmed inference and Fully Define',
          'host': platform.platform(), 'steps': [], 'module_11_started': False,
          'preserved_deferred_gates': ['6.1b', '6.4c', '7.4c'],
          'blocked_acceptance': ['Physical input devices and assistive technologies', 'Installed/signed builds', 'Windows/Linux', 'Full resource and whole-course acceptance corpus']}
if sys.platform == 'darwin':
    record['physical_ram_bytes'] = int(subprocess.check_output(['sysctl', '-n', 'hw.memsize'], text=True))

def save():
    (FOLDER / 'verification.json').write_text(json.dumps(record, indent=2) + '\n')

def run(name, command, env=None, timeout=1800):
    print('Running ' + name, flush=True)
    started = time.monotonic()
    with (FOLDER / (name + '.txt')).open('w') as log:
        try:
            code = subprocess.run(command, cwd=ROOT, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=timeout).returncode
        except subprocess.TimeoutExpired:
            code = 124
    record['steps'].append({'name': name, 'command': command, 'exit_code': code, 'seconds': time.monotonic() - started})
    save()
    return code

def require(name, command, env=None, timeout=1800):
    if run(name, command, env, timeout):
        record['status'] = 'Fail'
        save()
        raise SystemExit('Fail: ' + name)

baseline = ROOT / 'docs/testing/artifacts/core-modeling/cm3-completion-20261031/verification/20261101T124037.301140Z'
prior = json.loads((baseline / 'source-identity.json').read_text())['inputs']
differences = sorted(p for p in set(prior) | set(frozen) if prior.get(p) != frozen.get(p))
assert differences == ['tests/core_modeling/dimension_contracts.cpp'], differences
prior_result = json.loads((baseline / 'verification.json').read_text())
required = {'build', 'ctest', 'format', 'changed-source-analysis', 'architecture', 'research-isolation', 'coverage', 'specifications'}
assert {s['name'] for s in prior_result['steps'] if s['exit_code'] == 0} >= required
record['baseline'] = str(baseline.relative_to(ROOT))
record['baseline_validation'] = '76 enabled tests and all static/structural checks passed; only the native/widget dimension test repaint changed afterward'
record['changed_since_baseline'] = differences
save()
require('build', ['cmake', '--build', '--preset', 'macos-debug'])
require('affected-tests', ['ctest', '--preset', 'macos-debug', '-R', '^cm3.dimension_', '--output-on-failure'])
require('format', [sys.executable, 'tools/format_sources.py'])
entries = json.loads((ROOT / 'build/macos-debug/compile_commands.json').read_text())
filtered = [e for e in entries if e['file'].endswith('/tests/core_modeling/dimension_contracts.cpp')]
assert len(filtered) == 1
(FOLDER / 'analysis-commands.json').write_text(json.dumps(filtered, indent=2) + '\n')
require('affected-source-analysis', [sys.executable, '-u', 'tools/static_analysis.py', str(FOLDER / 'analysis-commands.json')])
require('coverage', [sys.executable, 'tools/check_core_modeling.py'])
require('specifications', [sys.executable, 'docs/scripts/check_specs.py'])
if '--native' in sys.argv:
    if sys.platform != 'darwin':
        record['status'] = 'Blocked: native runner requires the available Mac'
        save()
        raise SystemExit(record['status'])
    env = os.environ.copy()
    env.pop('QT_QPA_PLATFORM', None)
    env['CAD_NATIVE_TESTS'] = '1'
    for percent, factor in [(100, '.5'), (125, '.625'), (150, '.75'), (175, '.875'), (200, None)]:
        artifacts = FOLDER / f'native-{percent}'
        artifacts.mkdir()
        env['CAD_ARTIFACT_DIR'] = str(artifacts)
        env['CAD_EXPECTED_DPR'] = str(percent / 100)
        if factor:
            env['QT_SCALE_FACTOR'] = factor
        else:
            env.pop('QT_SCALE_FACTOR', None)
        require(f'completion-native-{percent}', [str(ROOT / 'build/macos-debug/cm3_completion_contracts')], env, 300)
        require(f'dimensions-native-{percent}', [str(ROOT / 'build/macos-debug/cm3_dimension_contracts'), 'identity', 'native'], env, 300)
        if percent in [100, 200]:
            require(f'workspace-native-{percent}', [str(ROOT / 'build/macos-debug/module4_workspace'), 'nativeWorkspace'], env, 240)
            require(f'arcs-native-{percent}', [str(ROOT / 'build/macos-debug/module3_contracts'), 'nativeArcs'], env, 240)
else:
    record['native'] = 'Not run; pass --native on the available Mac'
record['source_unchanged'] = inputs() == frozen
record['status'] = 'Pass: development-build scope; blocked acceptance remains separate' if record['source_unchanged'] else 'Fail: source inputs changed'
save()
if not record['source_unchanged']:
    raise SystemExit(record['status'])
print(record['status'], flush=True)
