#!/usr/bin/env python3
"""Verify the CM3 axis/ordinate/arc-length slice; never whole CM3 acceptance."""
import datetime
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
if sys.platform != 'darwin':
    raise SystemExit('Blocked: native evidence runner requires the available Mac')
FOLDER = ROOT/'docs/testing/artifacts/core-modeling'/datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ')/'cm3-associated'
FOLDER.mkdir(parents=True)
print(FOLDER, flush=True)

def inputs():
    paths = [ROOT/'CMakeLists.txt', ROOT/'CMakePresets.json', ROOT/'.clang-format', ROOT/'docs/scripts/check_specs.py']
    for top in ['apps', 'src', 'tests', 'tools', 'cmake']:
        paths += [p for p in (ROOT/top).rglob('*') if p.is_file() and p.suffix in {'.cpp','.hpp','.h','.py','.swift','.cmake','.json','.patch'}]
    return {str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(set(paths))}

frozen = inputs()
identity = {'branch':subprocess.check_output(['git','branch','--show-current'],cwd=ROOT,text=True).strip(),
            'head':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
            'sha256':hashlib.sha256(json.dumps(frozen,sort_keys=True).encode()).hexdigest(), 'inputs':frozen}
(FOLDER/'source-identity.json').write_text(json.dumps(identity,indent=2)+'\n')
record = {'status':'In progress','scope':'CM3 CenterlineDiameter (point/parallel profile line), shared-datum per-point ordinates and circular ArcLength, plus prior dimension regressions on the available Mac',
          'host':platform.platform(),'physical_ram_bytes':int(subprocess.check_output(['sysctl','-n','hw.memsize'],text=True)),
          'steps':[], 'static_scope':'Changed production translation units plus all CM3 test translation units; headers analyzed through their real consumers',
          'native_scope':'All CM3 editor modes at five effective scales; native workspace/arc regressions at endpoint scales; other core-modeling native matrices retain separate historical scope',
          'preserved_deferred_gates':['6.1b','6.4c','7.4c'],'module_11_started':False,
          'remaining':['Driving multientity path length; general direction/increment/drawing marking controls', 'Automatic relations/inference, Fully Define and constrained dragging', 'Whole-course recovery/resource/installed/physical input/accessibility and Windows/Linux acceptance']}
red = Path('/private/tmp/cm3-axis-upstream-red.txt')
if red.is_file():
    shutil.copyfile(red,FOLDER/'axis-upstream-red.txt')
    record['development_regression'] = 'axis-upstream-red.txt records the earlier unsigned-distance adapter; it is not a result for this final source identity'
def save(): (FOLDER/'verification.json').write_text(json.dumps(record,indent=2)+'\n')
def run(name, command, env=None, timeout=1800):
    print('Running '+name,flush=True)
    started=time.monotonic()
    with (FOLDER/(name+'.txt')).open('w') as log:
        try: code=subprocess.run(command,cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=timeout).returncode
        except subprocess.TimeoutExpired: code=124
        except KeyboardInterrupt:
            record['status']='Interrupted';save();raise
    record['steps'].append({'name':name,'command':command,'exit_code':code,'seconds':time.monotonic()-started});save()
    return code
def require(name, command, env=None, timeout=1800):
    if run(name, command, env, timeout):
        record['status']='Fail';save();raise SystemExit('Fail: '+name)
def native(name, binary, method, env):
    command=[str(ROOT/'build/macos-debug'/binary),method]
    if run(name,command,env,240):
        log=(FOLDER/(name+'.txt')).read_text()
        failures=[line for line in log.splitlines() if line.startswith('FAIL!')]
        activation = failures and all('QTest::qWaitForWindowActive(' in line and 'returned FALSE' in line for line in failures)
        if not activation:
            record['status']='Fail';save();raise SystemExit('Fail: '+name)
        record.setdefault('retained_activation_retries',[]).append({'failed_step':name,'reason':'Window activation only; original output retained'})
        save()
        require(name+'-activation-retry',command,env,240)

save()
require('configure',['cmake','--preset','macos-debug'])
require('build',['cmake','--build','--preset','macos-debug'])
require('ctest',['ctest','--preset','macos-debug','--output-on-failure'])
require('format',[sys.executable,'tools/format_sources.py'])
selected={'src/sketch/Sketch.cpp','src/sketch/Dimensions.cpp','src/sketch/Solver.cpp','src/persistence/ProjectStore.cpp',
          'apps/desktop/DimensionUI.cpp','apps/desktop/SketchPanel.cpp','src/render/SketchView.cpp',
          'tests/core_modeling/associated_dimensions.cpp','tests/core_modeling/dimensions.cpp','tests/core_modeling/dimension_contracts.cpp'}
entries=json.loads((ROOT/'build/macos-debug/compile_commands.json').read_text())
filtered=[e for e in entries if str(Path(e['file']).resolve().relative_to(ROOT)) in selected]
assert {str(Path(e['file']).resolve().relative_to(ROOT)) for e in filtered}==selected
(FOLDER/'analysis-commands.json').write_text(json.dumps(filtered,indent=2)+'\n')
require('changed-source-analysis',[sys.executable,'-u','tools/static_analysis.py',str(FOLDER/'analysis-commands.json')])
require('architecture',[sys.executable,'tools/check_architecture.py','build/macos-debug','--output',str(FOLDER/'architecture.json')])
require('research-isolation',[sys.executable,'tools/check_isolation.py'])
require('coverage',[sys.executable,'tools/check_core_modeling.py'])
require('specifications',[sys.executable,'docs/scripts/check_specs.py'])
env=os.environ.copy();env.pop('QT_QPA_PLATFORM',None);env['CAD_NATIVE_TESTS']='1'
for percent,factor in [(100,'.5'),(125,'.625'),(150,'.75'),(175,'.875'),(200,None)]:
    artifacts=FOLDER/f'native-{percent}';artifacts.mkdir();env['CAD_ARTIFACT_DIR']=str(artifacts)
    env['CAD_EXPECTED_DPR']=str(percent/100)
    if factor: env['QT_SCALE_FACTOR']=factor
    else: env.pop('QT_SCALE_FACTOR',None)
    native(f'cm3-native-{percent}','cm3_dimension_contracts','native',env)
    if percent in [100,200]:
        native(f'workspace-native-{percent}','module4_workspace','nativeWorkspace',env)
        native(f'arcs-native-{percent}','module3_contracts','nativeArcs',env)
record['source_unchanged']=inputs()==frozen
record['status']='Pass: associated-dimension slice; whole CM3 acceptance remains open' if record['source_unchanged'] else 'Fail: source inputs changed'
save()
if not record['source_unchanged']:raise SystemExit(record['status'])
print(record['status'],flush=True)
