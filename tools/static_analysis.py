#!/usr/bin/env python3
"""Run Clang's analyzer against project-owned C++ sources with their actual compile flags."""
import concurrent.futures
import json
import os
import re
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
entries = json.loads(Path(sys.argv[1]).read_text())


def analyze_entry(entry):
    source = Path(entry['file']).resolve()
    if not source.is_relative_to(ROOT):
        return None
    if source.relative_to(ROOT).parts[0] not in {'tests', 'apps', 'src'}:
        return None
    command = shlex.split(entry['command'])
    clean = []
    skip = False
    for arg in command:
        if skip:
            skip = False
            continue
        if arg == '-o':
            skip = True
            continue
        if arg == '-c':
            continue
        clean.append(arg)
    with tempfile.TemporaryDirectory(prefix='cad-step-zero-analyzer-') as temp:
        clean += ['--analyze', '-Xanalyzer', '-analyzer-output=text', '-o', str(Path(temp) / 'analysis.txt')]
        result = subprocess.run(clean, cwd=entry['directory'], text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=120)
        if result.stdout:
            print(result.stdout, end='', flush=True)
        if result.returncode or re.search(r':\d+:\d+: warning:', result.stdout):
            return f"Fail: analyzer reported an error or warning in {source.relative_to(ROOT)}"
    print('Analyzed', str(source.relative_to(ROOT)), flush=True)
    return None


workers = min(8, os.cpu_count() or 4)
with concurrent.futures.ThreadPoolExecutor(max_workers=workers) as executor:
    futures = [executor.submit(analyze_entry, e) for e in entries]
    for f in concurrent.futures.as_completed(futures):
        err = f.result()
        if err:
            raise SystemExit(err)

