#!/usr/bin/env python3
"""Preserve ordered Modules 8-10 available-Mac evidence; stop before Module 11."""
import argparse,datetime,hashlib,json,os,platform,shutil,subprocess,sys,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--native',action='store_true');p.add_argument('--clean',action='store_true');a=p.parse_args()
if sys.platform!='darwin':raise SystemExit('Blocked: provision Windows/Linux native runners')
base=ROOT/'docs/testing/artifacts/module-10';folder=base/'runs'/datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ');folder.mkdir(parents=True)
def inputs():
 paths=[ROOT/'CMakeLists.txt',ROOT/'CMakePresets.json',ROOT/'.clang-format']
 for top in ('apps','src','tests','tools','cmake'):
  paths += [f for f in (ROOT/top).rglob('*') if f.is_file() and f.suffix in ('.cpp','.hpp','.h','.py','.swift','.cmake','.json','.patch')]
 return {str(f.relative_to(ROOT)):hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted(set(paths))}
frozen=inputs()
record={'scope':'Modules 8-10 ordered implementation and available-Mac verification; stop before Module 11','status':'In progress','artifacts':str(folder.relative_to(ROOT)),'steps':[],
 'host':{'platform':platform.platform(),'machine':platform.machine(),'physical_ram_bytes':int(subprocess.check_output(['sysctl','-n','hw.memsize'],text=True)),'cpu':subprocess.check_output(['sysctl','-n','machdep.cpu.brand_string'],text=True).strip()},
 'blocked':['Prior Module 6.1b and 6.4c deferred parser/resource/platform gates','Prior Module 7.4c full resource/platform gate','Windows/Linux/minimum macOS/other GPU/installed release builds','VoiceOver/NVDA/Orca and physical device acceptance','Full physical/shared/GPU/FPS and retention budgets (Module 11 not started)','Physical full-volume/power-loss and other native filesystem matrix']}
def save():
 data=json.dumps(record,indent=2)+'\n';(folder/'verification.json').write_text(data);(base/'verification.json').write_text(data)
def run(name,command,env=None,timeout=600):
 print('Running',name,flush=True);start=time.monotonic()
 try:
  with (folder/(name+'.txt')).open('w') as log:r=subprocess.run(command,cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=timeout)
  code=r.returncode
 except subprocess.TimeoutExpired:code=124
 record['steps'].append({'name':name,'command':command,'exit_code':code,'seconds':time.monotonic()-start});save()
 if code:record['status']='Fail';save();raise SystemExit(f'Fail: {name}; see {folder}')
save();run('configure',['cmake','--preset','macos-debug']);run('build',['cmake','--build','--preset','macos-debug']);run('ctest',['ctest','--preset','macos-debug','--output-on-failure'],timeout=1800)
for name in ('module8-B5.pdf','module8-B5.json','module10-fault-matrix.json','module2-fault-matrix.json'):
 shutil.copy2(ROOT/'build/macos-debug'/name,folder/name)
run('pdf-inspection',['swift','-module-cache-path','/private/tmp/cad-pdf-swift-cache','tools/inspect_drawing_pdf.swift',str(folder/'module8-B5.pdf'),str(folder/'pdf')])
run('format',[sys.executable,'tools/format_sources.py']);run('static-analysis',[sys.executable,'tools/static_analysis.py','build/macos-debug/compile_commands.json'])
run('ownership',[sys.executable,'tools/check_architecture.py','build/macos-debug','--output',str(folder/'architecture.json')]);run('research-isolation',[sys.executable,'tools/check_isolation.py'])
run('offline',[sys.executable,'tools/offline_workflows.py','--output',str(folder/'offline')],timeout=600)
if a.native:
 env=os.environ.copy();env.pop('QT_QPA_PLATFORM',None);env['CAD_NATIVE_TESTS']='1'
 for percent,factor in [(100,'.5'),(125,'.625'),(150,'.75'),(175,'.875'),(200,None)]:
  output=folder/f'native-{percent}';output.mkdir();env['CAD_ARTIFACT_DIR']=str(output)
  if factor:env['QT_SCALE_FACTOR']=factor
  else:env.pop('QT_SCALE_FACTOR',None)
  for module,target,case in [(8,'module8_contracts','nativeDrawing'),(9,'module9_interaction','nativeInteraction')]:run(f'native-module{module}-{percent}',[str(ROOT/'build/macos-debug'/target),case],env,180)
 env.pop('QT_SCALE_FACTOR',None);env['CAD_ARTIFACT_DIR']=str(folder)
 for module,case in [(7,'nativeAssembly'),(6,'nativeExchange'),(5,'nativeHistory'),(4,'nativeFeatures'),(3,'nativeSketches'),(2,'nativeDocuments'),(1,'nativeInteraction')]:run(f'native-module{module}-regression',[str(ROOT/f'build/macos-debug/module{module}_contracts'),case],env,240)
if a.clean:run('clean-reproduction',[sys.executable,'tools/clean_reproduce.py','--module','10','--output',str(folder/'clean-reproduction-details.txt')],timeout=2400)
if inputs()!=frozen:record['status']='Fail: tested source inputs changed';save();raise SystemExit(record['status'])
identity={'git_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'branch':subprocess.check_output(['git','branch','--show-current'],cwd=ROOT,text=True).strip(),'dirty_paths':subprocess.check_output(['git','status','--short'],cwd=ROOT,text=True).splitlines(),'inputs':frozen,'sha256':hashlib.sha256(json.dumps(frozen,sort_keys=True).encode()).hexdigest()}
(folder/'source-identity.json').write_text(json.dumps(identity,indent=2)+'\n');record['status']='Pass: requested available-Mac checks; full acceptance Blocked';save();print(record['status'],flush=True)
