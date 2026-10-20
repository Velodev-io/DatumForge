#!/usr/bin/env python3
"""Real production-store process/filesystem fault tests; every root is disposable."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import select
import subprocess
import tempfile
import time

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('executable',type=Path)
p.add_argument('--output',type=Path)
a=p.parse_args();exe=str(a.executable.resolve());results=[]

def run(root,action,phase='',expected=0):
    cmd=[exe,str(root),action,*([phase] if phase else [])]
    r=subprocess.run(cmd,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=15)
    assert r.returncode==expected,(cmd,r.returncode,r.stdout)
    return r.stdout

def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def query(root):return json.loads(run(root,'query').splitlines()[-1])
def make(root,kind='Part'):
    (root/'.cad-test-root').write_text('disposable\n')
    recovery=root/'cad-module2-recovery';recovery.mkdir();(recovery/'.cad-test-root').write_text('disposable\n')
    run(root,'init',kind)
def check(name,fn):
    start=time.monotonic()
    with tempfile.TemporaryDirectory(prefix='cad-module2-fault-') as d:
        root=Path(d);fn(root)
    results.append({'case':name,'status':'Pass','seconds':time.monotonic()-start})
    print('Pass:',name,flush=True)
def edit_case(root,kind,phase,kill=False):
    make(root,kind);before=digest(root/'saved.cadproj')
    if kill:paused(root,'edit',phase)
    else:run(root,'edit',phase,70)
    q=query(root);committed=phase.endswith('after_commit')
    assert q['revision']==(3 if committed else 2),q
    assert q['operation']['durable']==committed,q
    assert digest(root/'saved.cadproj')==before
    run(root,'edit');assert query(root)['revision']==3

def paused(root,action,phase):
    process=subprocess.Popen([exe,str(root),action,'pause_'+phase],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
    try:
        ready,_,_=select.select([process.stdout],[],[],10);assert ready,'No boundary notification'
        line=process.stdout.readline();assert line.startswith('BOUNDARY '),line
        process.kill();process.wait(timeout=5);assert process.returncode<0
    finally:
        if process.poll() is None:process.kill();process.wait(timeout=5)

def save_case(root,kind,phase,kill=False):
    make(root,kind);before=digest(root/'saved.cadproj');run(root,'edit')
    if kill:paused(root,'save',phase)
    else:run(root,'save',phase,70)
    q=query(root);published=phase in ('after_publication','save_recorded')
    assert q['revision']==3 and q['saved_revision']==(3 if published else 2),q
    assert q['save_operation']['durable']==published,q
    info=json.loads(run(root,'inspect_saved').splitlines()[-1]);assert info['saved_revision']==(3 if published else 2)
    if not published:assert digest(root/'saved.cadproj')==before
    else:assert digest(root/'saved.cadproj.bak')==before
    assert not list(root.glob('.cad-snapshot-*')),'Owned abandoned snapshot was not cleaned'

def lock_case(root,hard=False,replace=False):
    make(root);alias=root/'alias.cadproj'
    if hard:os.link(root/'saved.cadproj',alias)
    else:alias.symlink_to(root/'saved.cadproj')
    owner=subprocess.Popen([exe,str(root),'save_and_lock' if replace else 'lock'],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
    try:
        ready,_,_=select.select([owner.stdout],[],[],10);assert ready
        assert owner.stdout.readline().startswith('LOCKED')
        assert 'ProjectLocked' in run(root,'open_alias',expected=1)
    finally:owner.kill();owner.wait(timeout=5)
    run(root,'open_alias')

def corrupt_case(root,mode):
    make(root);run(root,mode);before=digest(root/'saved.cadproj')
    output=run(root,'open',expected=1);assert ('IncompatibleSchema' if mode in ('corrupt_future','corrupt_trigger') else 'InvalidProject') in output,output
    assert digest(root/'saved.cadproj')==before

def quota_case(root):
    make(root);before=digest(root/'saved.cadproj');output=run(root,'disk_full',expected=1)
    assert 'StorageFull' in output,output
    q=query(root);assert q['revision']==2 and not q['operation']['durable']
    assert digest(root/'saved.cadproj')==before

def permission_case(root):
    make(root);run(root,'edit');before=digest(root/'saved.cadproj');root.chmod(0o500)
    try:assert 'PermissionDenied' in run(root,'save',expected=1)
    finally:root.chmod(0o700)
    assert query(root)['revision']==3 and query(root)['saved_revision']==2
    assert digest(root/'saved.cadproj')==before

def migration_case(root):
    make(root);run(root,'legacy');before=digest(root/'saved.cadproj');assert 'IncompatibleSchema' in run(root,'migrate',expected=1)
    assert digest(root/'saved.cadproj')==before and not (root/'migrated.cadproj').exists()

for kind in ('Part','Assembly','Drawing'):
    for phase in ('before_commit','after_commit'):
        check(f'{kind}: exit {phase}',lambda root,k=kind,p=phase:edit_case(root,k,p))
    for phase in ('snapshot_chunk','snapshot_validated','before_publication','after_publication','save_recorded'):
        check(f'{kind}: exit {phase}',lambda root,k=kind,p=phase:save_case(root,k,p))
for phase in ('before_commit','after_commit'):
    check('SIGKILL '+phase,lambda root,p=phase:edit_case(root,'Part',p,True))
for phase in ('before_publication','after_publication'):
    check('SIGKILL '+phase,lambda root,p=phase:save_case(root,'Part',p,True))
check('live writer: symlink alias',lambda root:lock_case(root))
check('live writer: hardlink alias',lambda root:lock_case(root,True))
check('writer identity survives snapshot replacement and old hardlink',lambda root:lock_case(root,True,True))
check('future schema refused unchanged',lambda root:corrupt_case(root,'corrupt_future'))
check('missing required object refused unchanged',lambda root:corrupt_case(root,'corrupt_missing'))
check('altered schema trigger refused unchanged',lambda root:corrupt_case(root,'corrupt_trigger'))
check('invalid semantic history refused unchanged',lambda root:corrupt_case(root,'corrupt_delta'))
check('real SQLITE_FULL from page quota',quota_case)
check('real denied destination write',permission_case)
check('migration stages a copy and refuses unsupported converter',migration_case)
report={'scope':'Production storage library on actual local files/processes; physical power loss and Windows/Linux are Blocked',
        'cases':results,'status':'Pass','count':len(results),'physical_disk_exhaustion':'Not run; SQLite page quota produces actual SQLITE_FULL without filling user storage'}
if a.output:a.output.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({'status':'Pass','cases':len(results)}))
