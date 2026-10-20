#!/usr/bin/env python3
"""Run Clang's analyzer against project-owned C++ sources with their actual compile flags."""
import json
import re
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile
ROOT=Path(__file__).resolve().parents[1]
entries=json.loads(Path(sys.argv[1]).read_text())
for entry in entries:
    source=Path(entry['file']).resolve()
    if not source.is_relative_to(ROOT):continue
    if source.relative_to(ROOT).parts[0] not in {'tests','apps','src'}:continue
    command=shlex.split(entry['command']);clean=[];skip=False
    for arg in command:
        if skip:skip=False;continue
        if arg=='-o':skip=True;continue
        if arg=='-c':continue
        clean.append(arg)
    with tempfile.TemporaryDirectory(prefix='cad-step-zero-analyzer-') as temp:
        clean+=['--analyze','-Xanalyzer','-analyzer-output=text','-o',str(Path(temp)/'analysis.txt')]
        result=subprocess.run(clean,cwd=entry['directory'],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120)
        if result.stdout:print(result.stdout,end='',flush=True)
        if result.returncode or re.search(r':\d+:\d+: warning:',result.stdout):raise SystemExit('Fail: analyzer reported an error or warning in '+str(source.relative_to(ROOT)))
    print('Analyzed',str(source.relative_to(ROOT)),flush=True)
