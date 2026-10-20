#!/usr/bin/env python3
"""Real bundled SQLite, native subprocess exits, disposable files. No mocks."""
import hashlib
import json
from pathlib import Path
import sqlite3
import subprocess
import sys
import tempfile

EXECUTABLE = str(Path(sys.argv[1]).resolve())
results = []
with tempfile.TemporaryDirectory(prefix='cad-step-zero-') as directory:
    root = Path(directory)
    (root / '.cad-test-root').write_text('local.solidworks-clone.test\n')
    def call(action, *args, expected=0):
        result = subprocess.run([EXECUTABLE, str(root), action, *args], capture_output=True, text=True, timeout=15)
        results.append({'action': action, 'arguments': args, 'exit': result.returncode,
                        'stdout': result.stdout.strip(), 'stderr': result.stderr.strip()})
        assert result.returncode == expected, results[-1]
        return result.stdout
    def revision(expected, commands):
        assert f'revision={expected} commands={commands}' in call('query')
    def digest(name):
        return hashlib.sha256((root / name).read_bytes()).hexdigest()
    call('init')
    call('save', '')
    original = digest('saved.cadproj')
    call('commit', 'before_commit', 'a', 'width=80', expected=70)
    revision(0, 0)
    for phase,op,expected_revision in [('pause_before_commit','kill-before',0),('pause_after_commit','kill-after',1)]:
        child=subprocess.Popen([EXECUTABLE,str(root),'commit',phase,op,'kill-fixture'],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        try:
            assert child.stdout.readline().strip()=='BOUNDARY'
            child.kill();child.wait(timeout=5)
        finally:
            if child.poll() is None:child.kill();child.wait(timeout=5)
        revision(expected_revision,expected_revision)
        results.append({'action':'external-kill','phase':phase,'exit':child.returncode,'recovered_revision':expected_revision})
    call('commit', 'after_commit', 'a', 'width=80', expected=71)
    revision(2, 2)
    call('commit', '', 'a', 'width=80')  # Lost acknowledgement: lookup/dedup, no second effect.
    revision(2, 2)
    call('commit', '', 'a', 'width=81', expected=1)
    revision(2, 2)
    call('save', 'during_snapshot', expected=72)
    assert digest('saved.cadproj') == original
    call('save', 'write_error', expected=1)
    assert digest('saved.cadproj') == original
    call('save', 'permission_error', expected=1)
    assert digest('saved.cadproj') == original
    call('save', 'after_publication', expected=73)
    assert 'revision=2' in call('inspect')
    assert not (root / 'saved.cadproj-wal').exists()
    assert not (root / 'saved.cadproj-shm').exists()
    call('commit', 'disk_full', 'b', 'width=82', expected=1)
    revision(2, 2)
    call('object')
    call('missing', expected=1)
    call('gc')
    call('save', '')
    with sqlite3.connect(root / 'saved.cadproj') as db:
        assert db.execute("SELECT count(*) FROM objects WHERE kind='cache'").fetchone()[0] == 0
        assert db.execute("SELECT length(payload) FROM objects WHERE id='source'").fetchone()[0] == 4194304
    call('migrate_fail', expected=1)
    revision(2, 2)
    # Real chmod denial, distinct from injectable error contract.
    root.chmod(0o500)
    try:
        call('save', '', expected=1)
    finally:
        root.chmod(0o700)
    revision(2, 2)
    (root / 'alias.sqlite').symlink_to(root / 'working.sqlite')
    (root / 'hardlink.sqlite').hardlink_to(root / 'working.sqlite')
    holder = subprocess.Popen([EXECUTABLE, str(root), 'lock', 'working.sqlite'], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    try:
        assert holder.stdout.readline().strip() == 'LOCKED'
        call('lock', 'alias.sqlite', expected=1)
        call('lock', 'hardlink.sqlite', expected=1)
        call('commit', '', 'blocked-writer', 'must-not-commit', expected=1)
    finally:
        holder.terminate(); holder.wait(timeout=5)
    revision(2,2)
    # Unsupported future schema must not be modified by compatibility inspection.
    with sqlite3.connect(root / 'saved.cadproj') as db:
        db.execute('PRAGMA user_version=99')
    future = digest('saved.cadproj')
    call('inspect', expected=1)
    assert digest('saved.cadproj') == future
    # Production-root guard must reject even an existing application data directory.
    refused = subprocess.run([EXECUTABLE, str(Path.home()), 'init'], capture_output=True, text=True)
    assert refused.returncode == 1
    print(json.dumps({'status': 'Pass', 'checks': results,
                      'scope': 'local filesystem process-crash prototype; no physical power-loss guarantee'}, indent=2))
