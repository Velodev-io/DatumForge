#!/usr/bin/env python3
"""Run Clang's analyzer against our C++ spikes with their actual compile flags."""
import json
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile
entries=json.loads(Path(sys.argv[1]).read_text())
for entry in entries:
    if '/tests/' not in entry['file']:continue
    command=shlex.split(entry['command']);clean=[];skip=False
    for arg in command:
        if skip:skip=False;continue
        if arg=='-o':skip=True;continue
        if arg=='-c':continue
        clean.append(arg)
    with tempfile.TemporaryDirectory(prefix='cad-step-zero-analyzer-') as temp:
        clean+=['--analyze','-Xanalyzer','-analyzer-output=text','-o',str(Path(temp)/'analysis.txt')]
        subprocess.run(clean,cwd=entry['directory'],check=True)
    print('Analyzed',Path(entry['file']).name)
