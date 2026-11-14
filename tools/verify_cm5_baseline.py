#!/usr/bin/env python3
"""Freeze the current-source baseline for CM5 on HEAD 37501e8."""
import datetime
import hashlib
import json
from pathlib import Path
import platform
import re
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
TIMESTAMP = sys.argv[1] if len(sys.argv) > 1 else datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ')
FOLDER = ROOT / 'docs/testing/artifacts/core-modeling/cm5-baseline-20261114/verification' / TIMESTAMP
FOLDER.mkdir(parents=True, exist_ok=True)
print(FOLDER, flush=True)


def inputs():
    paths = [ROOT / name for name in ['CMakeLists.txt', 'CMakePresets.json', '.clang-format', 'docs/scripts/check_specs.py']]
    for top in ['apps', 'src', 'tests', 'tools', 'cmake']:
        paths += [p for p in (ROOT / top).rglob('*') if p.is_file() and p.suffix in {'.cpp', '.hpp', '.h', '.py', '.swift', '.cmake', '.json', '.patch'}]
    return {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(set(paths))}


frozen = inputs()
identity = {
    'branch': subprocess.check_output(['git', 'branch', '--show-current'], cwd=ROOT, text=True).strip(),
    'head': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
    'inputs': frozen
}
(FOLDER / 'source-identity.json').write_text(json.dumps(identity, indent=2) + '\n')
record = {
    'status': 'In progress',
    'scope': 'Current-source baseline freeze for CM5 on HEAD 37501e8 (macOS debug build)',
    'host': platform.platform(),
    'head': identity['head'],
    'branch': identity['branch'],
    'input_count': len(frozen),
    'steps': [],
    'module_11_started': False,
    'preserved_deferred_gates': ['6.1b', '6.4c', '7.4c'],
    'exact_geometry': 'Production exact engine tests; see scoped evidence and explicit geometry restrictions',
    'persistence': 'Production saved/recovery/history, upgrade-copy, and SQL binding/placeholder recheck verification',
    'native_verification': 'Not run: no new UI controls; headless worker/transport checks do not certify CM5.d/e',
    'full_acceptance': 'Blocked: full source-course/platform/device/installed/resource acceptance'
}


def save():
    (FOLDER / 'verification.json').write_text(json.dumps(record, indent=2) + '\n')


def require(name, command, timeout=1800):
    print('Running ' + name, flush=True)
    started = time.monotonic()
    with (FOLDER / (name + '.txt')).open('w') as log:
        try:
            proc = subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, timeout=timeout)
            code = proc.returncode
        except subprocess.TimeoutExpired:
            code = 124
    elapsed = time.monotonic() - started
    record['steps'].append({'name': name, 'command': command, 'exit_code': code, 'seconds': elapsed})
    if code:
        record['status'] = 'Fail: ' + name
    save()
    if code:
        raise SystemExit(record['status'])


save()
require('configure', ['cmake', '--preset', 'macos-debug'])
require('build', ['cmake', '--build', '--preset', 'macos-debug'])
require('ctest-cm5', ['ctest', '--preset', 'macos-debug', '-R', '^cm5\\.', '--output-on-failure'])
require('ctest-full', ['ctest', '--preset', 'macos-debug', '--output-on-failure'])
require('persistence-sql-recheck', ['ctest', '--preset', 'macos-debug', '-R', '^(module[356]\\.persistence|cm5\\.(store|repair))$', '--output-on-failure'])
require('format', [sys.executable, 'tools/format_sources.py'])

entries = json.loads((ROOT / 'build/macos-debug/compile_commands.json').read_text())
selected = {
    'src/features/ModelingIntent.cpp',
    'tests/core_modeling/feature_intent.cpp',
    'src/geometry/occt/Part.cpp',
    'src/geometry/occt/ModelingProfiles.cpp',
    'src/geometry/occt/ModelingFeatures.cpp',
    'src/geometry/occt/ModelingExtents.cpp',
    'tests/core_modeling/modeling_profiles.cpp',
    'tests/core_modeling/modeling_features.cpp',
    'tests/core_modeling/modeling_contracts.cpp',
    'src/features/ModelingRecords.cpp',
    'src/geometry/occt/Supports.cpp',
    'src/persistence/ProjectStore.cpp',
    'src/protocol/Client.cpp',
    'apps/cad-worker/main.cpp',
    'apps/desktop/MainWindow.cpp',
    'tests/core_modeling/modeling_records.cpp',
    'tests/core_modeling/modeling_store.cpp',
    'tests/core_modeling/modeling_repair.cpp',
    'src/geometry/occt/SketchCurve.cpp',
    'src/sketch/LineEditing.cpp',
    'tests/core_modeling/editing.cpp',
    'apps/desktop/SketchPanel.cpp',
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

current_inputs = inputs()
record['source_unchanged'] = (current_inputs == frozen)
record['status'] = 'Pass: CM5 current-source baseline on HEAD 37501e8' if record['source_unchanged'] else 'Fail: source inputs changed'
ctest_log = (FOLDER / 'ctest-full.txt').read_text() if (FOLDER / 'ctest-full.txt').is_file() else ''
m_pass = re.search(r'(\d+)%\s+tests\s+passed,\s+(\d+)\s+tests\s+failed\s+out\s+of\s+(\d+)', ctest_log)
disabled_count = len(re.findall(r'\(Disabled\)', ctest_log))
passed_count = (int(m_pass.group(3)) - int(m_pass.group(2))) if m_pass else 0
failed_count = int(m_pass.group(2)) if m_pass else 0
total_count = passed_count + failed_count + disabled_count
summary = {
    'status': 'Pass' if record['source_unchanged'] else 'Fail',
    'scope': 'Current-source baseline freeze for CM5 on HEAD 37501e8 (macOS debug build)',
    'head': identity['head'],
    'branch': identity['branch'],
    'source_inputs': len(frozen),
    'ctest': {
        'total': total_count,
        'passed': passed_count,
        'failed': failed_count,
        'disabled': disabled_count
    },
    'analysis_translation_units': len(selected),
    'source_hashes_match': record['source_unchanged'],
    'preserved_deferred_gates': ['6.1b', '6.4c', '7.4c'],
    'module_11_started': False,
    'native_interaction': 'Not run: CM5.d',
    'physical_8gb': 'Not run',
    'windows_linux': 'Not run'
}
(FOLDER / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
save()
if not record['source_unchanged']:
    raise SystemExit(record['status'])
print(record['status'], flush=True)
