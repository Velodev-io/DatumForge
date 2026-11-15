#!/usr/bin/env python3
"""Validate CM coverage/evidence and preserve the previously deferred release gates."""
import argparse
import hashlib
import json
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--sources', type=Path, help='Optional original PDF directory; never needed by product tests')
a = p.parse_args()
coverage = json.loads((ROOT/'docs/research/core-modeling-coverage.json').read_text())
auth = json.loads((ROOT/'docs/decisions/implementation-scope.json').read_text())
assert auth['core_modeling_authorization']['milestones'] == [f'CM{i}' for i in range(10)]
assert auth['stop_before_module'] == 11 and auth['stop_before_task'] == '11.1a'
assert max(auth['authorized_modules']) == 10
seen = set()
for row in coverage['entries']:
    assert row['id'] not in seen, row['id']
    seen.add(row['id'])
    assert row['milestone'] in {f'CM{i}' for i in range(1,10)}, row
    assert row['tasks'] == [row['milestone']+'.'+c for c in 'abcde'], row
    source = coverage['sources'][row['source']]
    assert row['pages'] and all(isinstance(n,int) and 1 <= n <= source['pages'] for n in row['pages']), row
    assert row['analytical_fixture'] and row['native_workflow'], row
    assert row['status'] in {'Not run','In progress','Blocked','Pass','Fail'}, row
    if row['status'] == 'Pass':
        assert row.get('acceptance_scope') and row['evidence'], row
    for item in row['evidence']:
        target=(ROOT/item).resolve()
        assert target.is_relative_to(ROOT) and target.is_file(), item
if a.sources:
    for row in coverage['sources'].values():
        path=a.sources/row['file']
        assert hashlib.sha256(path.read_bytes()).hexdigest() == row['sha256'], path
status = json.loads((ROOT/'docs/testing/task-status.json').read_text())
for key in ['6.1b','6.4c','7.4c']:
    assert status[key]['status'] == 'Blocked', key
baseline = json.loads((ROOT/'docs/testing/artifacts/module-10/deferred-gates.json').read_text())
for key in ['6.1b','6.4c','7.4c']:
    assert status[key] == baseline[key], 'Previously deferred gate changed: '+key
print(f'Pass: {len(seen)} CM requirements with page/task/fixture mappings; source/evidence bounds and preserved deferred gates')
