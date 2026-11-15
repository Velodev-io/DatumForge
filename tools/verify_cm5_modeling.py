#!/usr/bin/env python3
"""Verify CM5.b exact worker modeling; native/full physical acceptance stays separate."""
import datetime
import hashlib
import json
from pathlib import Path
import platform
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
FOLDER = ROOT / 'docs/testing/artifacts/core-modeling/cm5-b-20261103/verification' / datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ')
FOLDER.mkdir(parents=True)
print(FOLDER, flush=True)


def inputs():
    paths = [ROOT / name for name in ['CMakeLists.txt', 'CMakePresets.json', '.clang-format', 'docs/scripts/check_specs.py']]
    for top in ['apps', 'src', 'tests', 'tools', 'cmake']:
        paths += [p for p in (ROOT / top).rglob('*') if p.is_file() and p.suffix in {'.cpp', '.hpp', '.h', '.py', '.swift', '.cmake', '.json', '.patch'}]
    return {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(set(paths))}


frozen = inputs()
identity = {'branch': subprocess.check_output(['git', 'branch', '--show-current'], cwd=ROOT, text=True).strip(),
            'head': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(), 'inputs': frozen}
(FOLDER / 'source-identity.json').write_text(json.dumps(identity, indent=2) + '\n')
record = {'status': 'In progress', 'scope': 'CM5.b exact solid intent engine and read-only worker preview on available Mac',
          'host': platform.platform(), 'steps': [], 'module_11_started': False,
          'preserved_deferred_gates': ['6.1b', '6.4c', '7.4c'],
          'exact_geometry': 'Production exact engine tests; see scoped evidence and explicit geometry restrictions', 'persistence': 'Not run: CM5.c',
          'native_verification': 'Not run: no new UI controls; headless worker/transport checks do not certify CM5.d/e',
          'full_acceptance': 'Blocked: full source-course/platform/device/installed/resource acceptance'}


def save():
    (FOLDER / 'verification.json').write_text(json.dumps(record, indent=2) + '\n')


def require(name, command, timeout=1800):
    print('Running ' + name, flush=True)
    started = time.monotonic()
    with (FOLDER / (name + '.txt')).open('w') as log:
        try:
            code = subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, timeout=timeout).returncode
        except subprocess.TimeoutExpired:
            code = 124
    record['steps'].append({'name': name, 'command': command, 'exit_code': code, 'seconds': time.monotonic() - started})
    if code:
        record['status'] = 'Fail: ' + name
    save()
    if code:
        raise SystemExit(record['status'])


save()
require('build', ['cmake', '--build', '--preset', 'macos-debug'])
require('ctest', ['ctest', '--preset', 'macos-debug', '--output-on-failure'])
require('format', [sys.executable, 'tools/format_sources.py'])
entries = json.loads((ROOT / 'build/macos-debug/compile_commands.json').read_text())
selected = {
    'src/geometry/occt/Part.cpp', 'src/geometry/occt/ModelingProfiles.cpp',
    'src/geometry/occt/ModelingFeatures.cpp', 'src/geometry/occt/ModelingExtents.cpp',
    'src/persistence/ProjectStore.cpp', 'apps/cad-worker/main.cpp',
    'tests/core_modeling/modeling_profiles.cpp', 'tests/core_modeling/modeling_features.cpp',
    'tests/core_modeling/modeling_contracts.cpp',
}
filtered = [e for e in entries if str(Path(e['file']).resolve().relative_to(ROOT)) in selected]
assert {str(Path(e['file']).resolve().relative_to(ROOT)) for e in filtered} == selected
(FOLDER / 'analysis-commands.json').write_text(json.dumps(filtered, indent=2) + '\n')
require('changed-source-analysis', [sys.executable, '-u', 'tools/static_analysis.py', str(FOLDER / 'analysis-commands.json')])
require('architecture', [sys.executable, 'tools/check_architecture.py', 'build/macos-debug', '--output', str(FOLDER / 'architecture.json')])
require('research-isolation', [sys.executable, 'tools/check_isolation.py'])
require('coverage', [sys.executable, 'tools/check_core_modeling.py'])
require('specifications', [sys.executable, 'docs/scripts/check_specs.py'])
require('whitespace', ['git', 'diff', '--check'])
record['source_unchanged'] = inputs() == frozen
record['status'] = 'Pass: CM5.b available-Mac development-build engine/worker scope' if record['source_unchanged'] else 'Fail: source inputs changed'
save()
if not record['source_unchanged']:
    raise SystemExit(record['status'])
print(record['status'], flush=True)
