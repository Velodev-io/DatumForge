#!/usr/bin/env python3
"""Verify the authorized Centerpoint, 3 Point, Tangent/Normal Arc interactions without using a user project."""
import argparse,datetime,hashlib,json,os,platform,subprocess,sys,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--native',action='store_true');a=p.parse_args()
folder=ROOT/'docs/testing/artifacts/module-3/all-arc-runs'/datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ');folder.mkdir(parents=True)
if sys.platform!='darwin':raise SystemExit('Blocked: provision a native platform runner')
def inputs():
 paths=[ROOT/'CMakeLists.txt',ROOT/'CMakePresets.json',ROOT/'.clang-format']
 for top in ('apps','src','tests','tools','cmake'):
  paths += [f for f in (ROOT/top).rglob('*') if f.is_file() and f.suffix in ('.cpp','.hpp','.h','.py','.swift','.cmake','.json','.patch')]
 return {str(f.relative_to(ROOT)):hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted(set(paths))}
frozen=inputs()
r={'scope':'All three arc tools, endpoint relations and EDP line/normal-arc workflow; available-Mac scope; Module 11 unstarted','status':'In progress','host':platform.platform(),'steps':[],'blocked':['Physical mouse/trackpad and assistive technology','Other platforms, installed builds and full 8 GB resource budgets'],'not_run':['New clean-source/dependency rebuild; prior run retained separately','Ellipse/spline tangent sources and complete SolidWorks parity; source geometry types unavailable']}
def save():(folder/'verification.json').write_text(json.dumps(r,indent=2)+'\n')
def run(name,command,env=None,timeout=600):
 print('Running',name,flush=True);start=time.monotonic()
 with (folder/(name+'.txt')).open('w') as log:
  try:code=subprocess.run(command,cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=timeout).returncode
  except subprocess.TimeoutExpired:code=124
 r['steps'].append({'name':name,'command':command,'cwd':str(ROOT),'exit_code':code,'seconds':time.monotonic()-start,'log':name+'.txt'});save()
 if code:r['status']='Fail';save();raise SystemExit(f'Fail: {name}; see {folder}')
save();run('configure',['cmake','--preset','macos-debug']);run('build',['cmake','--build','--preset','macos-debug']);run('ctest',['ctest','--preset','macos-debug','--output-on-failure'],timeout=1800)
run('format',[sys.executable,'tools/format_sources.py'])
changed={'src/render/SketchView.cpp','apps/desktop/SketchPanel.cpp','apps/desktop/WorkspaceChrome.cpp','tests/module3/input.cpp','tests/module3/contracts.cpp','src/sketch/Solver.cpp','src/sketch/Sketch.cpp','tests/module3/solver.cpp'}
commands=json.loads((ROOT/'build/macos-debug/compile_commands.json').read_text());selected=[e for e in commands if str(Path(e['file']).resolve().relative_to(ROOT)) in changed]
assert len(selected)==len(changed)
(folder/'changed-compile-commands.json').write_text(json.dumps(selected,indent=2)+'\n')
run('changed-source-analysis',[sys.executable,'tools/static_analysis.py',str(folder/'changed-compile-commands.json')])
run('architecture',[sys.executable,'tools/check_architecture.py','build/macos-debug','--output',str(folder/'architecture.json')]);run('research-isolation',[sys.executable,'tools/check_isolation.py'])
if a.native:
 env=os.environ.copy();env.pop('QT_QPA_PLATFORM',None);env['CAD_NATIVE_TESTS']='1'
 for percent,factor in [(100,'.5'),(125,'.625'),(150,'.75'),(175,'.875'),(200,None)]:
  out=folder/f'native-{percent}';out.mkdir();env['CAD_ARTIFACT_DIR']=str(out)
  if factor:env['QT_SCALE_FACTOR']=factor
  else:env.pop('QT_SCALE_FACTOR',None)
  run(f'native-arcs-{percent}',[str(ROOT/'build/macos-debug/module3_contracts'),'nativeArcs'],env,180)
  if percent in (100,200):run(f'native-sketch-regression-{percent}',[str(ROOT/'build/macos-debug/module3_contracts'),'nativeSketches'],env,180)
if inputs()!=frozen:r['status']='Fail: source inputs changed';save();raise SystemExit(r['status'])
identity={'inputs':frozen,'sha256':hashlib.sha256(json.dumps(frozen,sort_keys=True).encode()).hexdigest(),'branch':subprocess.check_output(['git','branch','--show-current'],cwd=ROOT,text=True).strip(),'git_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()}
(folder/'source-identity.json').write_text(json.dumps(identity,indent=2)+'\n');r['status']='Pass: requested available-Mac checks; full acceptance Blocked';save();print(str(folder),flush=True);print(r['status'],flush=True)
