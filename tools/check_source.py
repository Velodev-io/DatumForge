#!/usr/bin/env python3
"""Whitespace/encoding supplement to the pinned clang-format command."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
files=[]
for folder in ['tests','tools','cmake','src','apps']:
    files.extend(p for p in (ROOT/folder).rglob('*') if p.suffix in {'.cpp','.hpp','.h','.py','.cmake'})
for path in files:
    text=path.read_text()
    assert text.endswith('\n'),f'Missing newline: {path}'
    assert all(line==line.rstrip() for line in text.splitlines()),f'Trailing whitespace: {path}'
print(f'Pass: whitespace/UTF-8 check for {len(files)} sources')
