#!/usr/bin/env python3
"""Preserve Module 6 exchange/portable-copy evidence on the available Mac."""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import time
from sample_memory import sample
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--native',action='store_true');p.add_argument('--clean',action='store_true');args=p.parse_args()
if sys.platform!='darwin':raise SystemExit('Blocked: provision native Windows/Linux verification runners')
base=ROOT/'docs/testing/artifacts/module-6'
folder=base/'runs'/datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ');folder.mkdir(parents=True)
record={'scope':'Module 6 available-Mac exchange and portable Part copies; stop before Module 7',
 'status':'In progress','artifacts':str(folder.relative_to(ROOT)),'steps':[],
 'host':{'platform':platform.platform(),'machine':platform.machine(),
 'physical_ram_bytes':int(subprocess.check_output(['sysctl','-n','hw.memsize'],text=True)),
 'cpu':subprocess.check_output(['sysctl','-n','machdep.cpu.brand_string'],text=True).strip()},
 'blocked':['Windows/Linux/other GPU/minimum macOS/installed builds','Physical-device and assistive-technology acceptance',
 'Unique shared/GPU memory and arbitrary adversarial kernel peak allocation guarantees',
 'Full B4 physical-memory budgets and large industrial corpus','Assembly/drawing closure (Modules 7–8)',
 'Native OS file-picker automation (Qt pickers are exercised on Cocoa)','Physical full-volume/power-loss storage matrix']}
def save():
 content=json.dumps(record,indent=2)+'\n';(folder/'verification.json').write_text(content);(base/'verification.json').write_text(content)
def inputs():
 paths=[ROOT/'CMakeLists.txt',ROOT/'CMakePresets.json',ROOT/'.clang-format']
 for sub in ('apps','src','tests','tools','cmake'):
  paths += [f for f in (ROOT/sub).rglob('*') if f.is_file() and f.suffix in ('.cpp','.hpp','.h','.py','.cmake','.json','.patch')]
 return {str(f.relative_to(ROOT)):hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted(set(paths))}
frozen=inputs();save()
def run(name,command,env=None,timeout=600,measure=False):
 print('Running',name,flush=True);start=time.monotonic();samples=[]
 with (folder/(name+'.txt')).open('w') as log:
  process=subprocess.Popen(command,cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT)
  while process.poll() is None:
   if time.monotonic()-start>timeout:process.kill();process.wait();break
   if measure:
    pids=[process.pid]
    for pid in list(pids):
     children=subprocess.run(['pgrep','-P',str(pid)],capture_output=True,text=True)
     pids += [int(v) for v in children.stdout.split()]
    samples.append({'elapsed_seconds':time.monotonic()-start,'processes':[sample(pid) for pid in pids]})
   time.sleep(.1)
  code=process.returncode
 record['steps'].append({'name':name,'command':command,'exit_code':code,'seconds':time.monotonic()-start})
 if samples:
  (folder/(name+'-memory.json')).write_text(json.dumps({'method':'proc_pid_rusage v2; 100 ms sampling; explicit test process and its worker children',
   'limits':'Test harness/worker sum is conservative and may double count shared allocations. Short phase peaks can be missed; no separate GPU or full B4 acceptance claim.',
   'maximum_sampled_sum_physical_footprint_bytes':max(sum(v.get('physical_footprint_bytes',0) for v in row['processes']) for row in samples),
   'samples':samples},indent=2)+'\n')
 if code:record['status']='Fail'
 save()
 if code:raise SystemExit(f'Fail: {name}; see {folder}')
run('configure',['cmake','--preset','macos-debug'])
run('build',['cmake','--build','--preset','macos-debug'])
run('ctest',['ctest','--preset','macos-debug','--output-on-failure'],timeout=1800)
import shutil
shutil.copy2(ROOT/'build/macos-debug/module6-b4.json',folder/'b4-correctness.json')
run('format',[sys.executable,'tools/format_sources.py'])
run('static-analysis',[sys.executable,'tools/static_analysis.py','build/macos-debug/compile_commands.json'])
run('ownership',[sys.executable,'tools/check_architecture.py','build/macos-debug','--output',str(folder/'architecture.json')])
run('research-isolation',[sys.executable,'tools/check_isolation.py'])
if args.native:
 env=os.environ.copy();env.pop('QT_QPA_PLATFORM',None);env['CAD_NATIVE_TESTS']='1'
 for percent,factor in [(100,'.5'),(125,'.625'),(150,'.75'),(175,'.875'),(200,None)]:
  directory=folder/f'native-{percent}';directory.mkdir();env['CAD_ARTIFACT_DIR']=str(directory)
  if factor:env['QT_SCALE_FACTOR']=factor
  else:env.pop('QT_SCALE_FACTOR',None)
  run(f'native-exchange-{percent}',[str(ROOT/'build/macos-debug/module6_contracts'),'nativeExchange'],env,180,True)
 env.pop('QT_SCALE_FACTOR',None);env['CAD_ARTIFACT_DIR']=str(folder)
 for module,case in [(5,'nativeHistory'),(4,'nativeFeatures'),(3,'nativeSketches'),(2,'nativeDocuments'),(1,'nativeInteraction')]:
  run(f'native-module{module}-regression',[str(ROOT/f'build/macos-debug/module{module}_contracts'),case],env,180)
if args.clean:
 run('clean-reproduction',[sys.executable,'tools/clean_reproduce.py','--module','6','--output',str(folder/'clean-reproduction-details.txt')],timeout=2400)
if inputs()!=frozen:
 record['status']='Fail: source inputs changed during verification';save();raise SystemExit(record['status'])
identity={'git_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
 'branch':subprocess.check_output(['git','symbolic-ref','--short','HEAD'],cwd=ROOT,text=True).strip(),
 'dirty_paths':subprocess.check_output(['git','status','--short'],cwd=ROOT,text=True).splitlines(),
 'inputs':frozen,'sha256':hashlib.sha256(json.dumps(frozen,sort_keys=True).encode()).hexdigest(),
 'B4_recipe_sha256':hashlib.sha256((ROOT/'tests/fixtures/B4.json').read_bytes()).hexdigest()}
(folder/'source-identity.json').write_text(json.dumps(identity,indent=2)+'\n')
record['status']='Pass: requested available-Mac checks; broader acceptance Blocked';save();print(record['status'],flush=True)
