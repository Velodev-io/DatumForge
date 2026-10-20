#!/usr/bin/env python3
"""Build and verify Module 2 on the available native Mac; preserve prior-module evidence."""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--native',action='store_true');args=p.parse_args()
if sys.platform!='darwin':raise SystemExit('Blocked: this runner has not been validated on Windows/Linux; native platform runners are required')
artifact=ROOT/'docs/testing/artifacts/module-2';artifact.mkdir(parents=True,exist_ok=True)
steps=[]
def run(name,command,env=None,timeout=600):
    print('Running',name,flush=True);start=time.monotonic()
    with (artifact/(name+'.txt')).open('w') as log:
        try:result=subprocess.run(command,cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=timeout);code=result.returncode
        except subprocess.TimeoutExpired:code=124;log.write('\nCommand timed out\n')
    steps.append({'name':name,'command':command,'exit_code':code,'seconds':time.monotonic()-start})
    record={'scope':'Available-Mac Module 2 document/persistence foundation; no full R1/platform acceptance',
            'recorded_at_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'steps':steps,
            'status':'Blocked' if code==77 else 'Fail' if code else 'In progress',
            'Windows':'Blocked: native host/compiler/filesystem unavailable',
            'Linux':'Blocked: native host/filesystem unavailable',
            'physical_power_loss':'Blocked: hardware/power-loss procedure unavailable',
            'physical_disk_exhaustion':'Not run: actual SQLITE_FULL is tested using a database page quota',
            'native_requested':args.native}
    (artifact/'verification.json').write_text(json.dumps(record,indent=2)+'\n')
    if code:raise SystemExit(f'{"Blocked" if code==77 else "Fail"}: {name} exit {code}; see saved log')

run('configure',['cmake','--preset','macos-debug'])
run('build',['cmake','--build','--preset','macos-debug'])
run('ctest',['ctest','--preset','macos-debug','--output-on-failure'],timeout=240)
shutil.copyfile(ROOT/'build/macos-debug/module2-fault-matrix.json',artifact/'fault-matrix.json')
run('format',[sys.executable,'tools/format_sources.py'])
run('static-analysis',[sys.executable,'-u','tools/static_analysis.py','build/macos-debug/compile_commands.json'])
run('ownership',[sys.executable,'tools/check_architecture.py','build/macos-debug','--output',str(artifact/'architecture.json')])
run('research-isolation',[sys.executable,'tools/check_isolation.py'])
if args.native:
    env=os.environ.copy();env.update(CAD_NATIVE_TESTS='1',CAD_ARTIFACT_DIR=str(artifact));env.pop('QT_SCALE_FACTOR',None)
    run('native-module2',[str(ROOT/'build/macos-debug/module2_contracts'),'nativeDocuments','-o',str(artifact/'native-module2.txt')+',txt'],env,60)
    # Regression: exercise the same Module 1 UI/viewport implementation, writing here.
    for percent,factor in [(100,'.5'),(125,'.625'),(150,'.75'),(175,'.875'),(200,None)]:
        if factor is None:env.pop('QT_SCALE_FACTOR',None)
        else:env['QT_SCALE_FACTOR']=factor
        run(f'native-module1-scale{percent}',[str(ROOT/'build/macos-debug/module1_contracts'),'nativeInteraction','-o',str(artifact/f'native-module1-scale{percent}.txt')+',txt'],env,60)
files=[ROOT/'CMakeLists.txt',ROOT/'CMakePresets.json',ROOT/'.clang-format']
for folder in ('apps','src','tests','tools','cmake'):
    files.extend(f for f in (ROOT/folder).rglob('*') if f.is_file() and f.suffix in ('.cpp','.hpp','.h','.py','.cmake','.json','.patch'))
hashes={str(f.relative_to(ROOT)):hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted(set(files))}
commit=subprocess.run(['git','rev-parse','--verify','HEAD'],cwd=ROOT,text=True,stdout=subprocess.PIPE,stderr=subprocess.DEVNULL)
branch=subprocess.check_output(['git','symbolic-ref','--short','HEAD'],cwd=ROOT,text=True).strip()
(artifact/'source-identity.json').write_text(json.dumps({'git_commit':commit.stdout.strip() if commit.returncode==0 else 'None: uncommitted initial repository','branch':branch,'inputs':hashes,'sha256':hashlib.sha256(json.dumps(hashes,sort_keys=True).encode()).hexdigest()},indent=2)+'\n')
record=json.loads((artifact/'verification.json').read_text());record['status']='Pass: requested available-Mac checks';(artifact/'verification.json').write_text(json.dumps(record,indent=2)+'\n')
print('Pass: available-Mac Module 2 verification; unrun platform/hardware checks remain Blocked',flush=True)
