#!/usr/bin/env python3
"""Exact Part/Assembly/Drawing crash matrix using the production store."""
import argparse,hashlib,json,select,subprocess,tempfile,time
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('executable',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args();exe=str(a.executable.resolve());results=[]
def run(root,action,phase='',expected=0):
 r=subprocess.run([exe,str(root),action,*([phase] if phase else [])],capture_output=True,text=True,timeout=40);assert r.returncode==expected,(action,phase,r.returncode,r.stdout,r.stderr);return r.stdout
def query(root):return json.loads(run(root,'query').splitlines()[-1])
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def kill(root,action,stage):
 process=subprocess.Popen([exe,str(root),action,'pause_'+stage],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
 try:
  ready,_,_=select.select([process.stdout],[],[],30);assert ready;assert process.stdout.readline().startswith('BOUNDARY ');process.kill();process.wait(timeout=5);assert process.returncode<0
 finally:
  if process.poll() is None:process.kill();process.wait()
for kind in ('Part','Assembly','Drawing'):
 for action,stages in [('edit',['before_compute','after_compute','before_commit','after_commit']),('save',['snapshot_chunk','snapshot_validated','before_publication','after_publication','save_recorded'])]:
  for stage in stages:
   start=time.monotonic()
   with tempfile.TemporaryDirectory(prefix='cad-module10-fault-') as d:
    root=Path(d);(root/'.cad-test-root').write_text('disposable\n');recovery=root/'cad-module10-recovery';recovery.mkdir();(recovery/'.cad-test-root').write_text('disposable\n');run(root,'init',kind);base=json.loads((root/'meta.json').read_text())['base'];before=digest(root/'saved.cadproj')
    if action=='save':run(root,'edit')
    kill(root,action,stage);q=query(root);committed=action=='save' or stage=='after_commit';published=action=='save' and stage in ('after_publication','save_recorded');assert q['revision']==base+int(committed),q;assert q['operation_count']==int(committed),q;assert q['saved_revision']==base+int(published),q
    if not published:assert digest(root/'saved.cadproj')==before
    else:assert digest(root/'saved.cadproj.bak')==before
    if committed:
     expected_volume=0.0000066 if kind=='Part' else .000006;assert abs(q['volume_m3']-expected_volume)<1e-12,q
     if kind=='Assembly':assert abs(q['translation_m']-.01)<1e-12,q
     if kind=='Drawing':assert abs(q['scale']-.4)<1e-12 and q['drawing_current'],q
    run(root,'edit');again=query(root);assert again['revision']==base+1 and again['operation_count']==1
    assert not list(root.glob('.cad-snapshot-*'))
   results.append({'kind':kind,'action':action,'stage':stage,'termination':'SIGKILL','status':'Pass','seconds':time.monotonic()-start});print('Pass',kind,action,stage,flush=True)
for stage in ('pdf_before_publication','pdf_after_publication'):
 with tempfile.TemporaryDirectory(prefix='cad-module10-pdf-fault-') as d:
  root=Path(d);(root/'.cad-test-root').write_text('disposable\n');recovery=root/'cad-module10-recovery';recovery.mkdir();(recovery/'.cad-test-root').write_text('disposable\n');run(root,'init','Drawing');prior=digest(root/'saved.pdf');cad_hash=digest(root/'saved.cadproj');run(root,'edit');kill(root,'pdf',stage);q=query(root);assert q['drawing_current'] and q['operation_count']==1;assert digest(root/'saved.cadproj')==cad_hash
  data=(root/'saved.pdf').read_bytes();assert data.startswith(b'%PDF-') and data.rstrip().endswith(b'%%EOF')
  if stage=='pdf_before_publication':assert digest(root/'saved.pdf')==prior
  else:assert digest(root/'saved.pdf')!=prior
  assert not list(root.glob('.cad-pdf-*')),'Owned orphan PDF staging was not recovered'
 results.append({'kind':'Drawing','action':'PDF export','stage':stage,'termination':'SIGKILL','status':'Pass'});print('Pass Drawing PDF',stage,flush=True)
report={'status':'Pass','count':len(results),'cases':results,'limits':'Actual local filesystem and process kill; physical power loss, full volume exhaustion and Windows/Linux remain Blocked'};a.output.write_text(json.dumps(report,indent=2)+'\n')
