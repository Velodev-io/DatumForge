#!/usr/bin/env python3
"""Verify CM4.c on the available development build, without claiming native acceptance."""
import hashlib
import json
import platform
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'docs/testing/artifacts/core-modeling/cm4-c-20261101'
OUT.mkdir(parents=True, exist_ok=True)


def inputs():
    paths = [ROOT / 'CMakeLists.txt', ROOT / 'CMakePresets.json', ROOT / '.clang-format']
    for top in ['src', 'apps', 'tests', 'tools', 'cmake']:
        paths += [p for p in (ROOT / top).rglob('*') if p.is_file() and p.suffix in
                  {'.cpp', '.hpp', '.h', '.py', '.cmake', '.json', '.patch'}]
    return {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted(set(paths))}


frozen = inputs()
identity = {'branch': subprocess.check_output(['git', 'branch', '--show-current'], cwd=ROOT, text=True).strip(),
            'head': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
            'inputs': frozen}
(OUT / 'source-identity.json').write_text(json.dumps(identity, indent=2) + '\n')
record = {'status': 'In progress', 'scope': 'CM4.c durable same-Part supports, unique topology evolution and explicit worker repair',
          'host': platform.platform(), 'steps': [], 'native_verification': 'Not run: native datum controls remain CM4.d',
          'full_acceptance': 'Blocked: CM4.e course/native workflows, installed/other-platform/device/resource matrices unrun',
          'preserved_deferred_gates': ['6.1b', '6.4c', '7.4c'], 'module_11_started': False}


def save():
    (OUT / 'verification.json').write_text(json.dumps(record, indent=2) + '\n')


def run(name, args, timeout=1800):
    print('Running ' + name, flush=True)
    start = time.monotonic()
    with (OUT / (name + '.txt')).open('w') as output:
        try:
            code = subprocess.run(args, cwd=ROOT, stdout=output, stderr=subprocess.STDOUT, timeout=timeout).returncode
        except subprocess.TimeoutExpired:
            code = 124
    record['steps'].append({'name': name, 'command': args, 'exit_code': code, 'seconds': time.monotonic() - start})
    save()
    if code:
        record['status'] = 'Fail'
        save()
        raise SystemExit('Fail: ' + name + '; see ' + str(OUT))


save()
run('configure-final', ['cmake', '--preset', 'macos-debug'])
run('build-final', ['cmake', '--build', '--preset', 'macos-debug'])
run('ctest', ['ctest', '--preset', 'macos-debug', '--output-on-failure'])
run('format', [sys.executable, 'tools/format_sources.py'])
database = json.loads((ROOT / 'build/macos-debug/compile_commands.json').read_text())
changed = set(subprocess.check_output(['git', 'diff', '--name-only'], cwd=ROOT, text=True).splitlines())
changed.update(subprocess.check_output(['git', 'ls-files', '--others', '--exclude-standard'], cwd=ROOT, text=True).splitlines())
selected = [entry for entry in database if str(Path(entry['file']).relative_to(ROOT)) in changed or
            Path(entry['file']).relative_to(ROOT).as_posix().startswith('tests/core_modeling/support')]
(OUT / 'analyzer-inputs.json').write_text(json.dumps(selected, indent=2) + '\n')
run('static-analysis', [sys.executable, '-u', 'tools/static_analysis.py', str(OUT / 'analyzer-inputs.json')])
run('architecture', [sys.executable, 'tools/check_architecture.py', 'build/macos-debug', '--output', str(OUT / 'architecture.json')])
run('coverage', [sys.executable, 'tools/check_core_modeling.py'])
run('research-isolation', [sys.executable, 'tools/check_isolation.py'])
run('specifications', [sys.executable, 'docs/scripts/check_specs.py'])
unchanged = inputs() == frozen
(OUT / 'source-integrity.txt').write_text(('Pass' if unchanged else 'Fail') + ': ' + str(len(frozen)) + ' source/configuration hashes checked\n')
if not unchanged:
    record['status'] = 'Fail: source inputs changed during verification'
    save()
    raise SystemExit(record['status'])
record['status'] = 'Pass'
record['analyzed_translation_units'] = len(selected)
record['source_hashes_checked'] = len(frozen)
save()
print('Pass: scoped CM4.c development verification; acceptance boundaries retained', flush=True)
