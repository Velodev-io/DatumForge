#!/usr/bin/env python3
"""Evidence for CM0 feasibility and the implemented CM1 CM2 straight-line editing and CM3 dimension slices, never full CM0-CM9 acceptance."""
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
ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--native', action='store_true')
a = p.parse_args()
if sys.platform != 'darwin':
    raise SystemExit('Blocked: provide the corresponding native platform evidence runner')
folder = ROOT/'docs/testing/artifacts/core-modeling'/datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ')/'verification'
folder.mkdir(parents=True)
print(str(folder), flush=True)
def inputs():
    paths = [ROOT/'CMakeLists.txt', ROOT/'CMakePresets.json', ROOT/'.clang-format', ROOT/'docs/scripts/check_specs.py']
    for top in ['apps','src','tests','tools','cmake']:
        paths += [f for f in (ROOT/top).rglob('*') if f.is_file() and f.suffix in {'.cpp','.hpp','.h','.py','.swift','.cmake','.json','.patch'}]
    return {str(f.relative_to(ROOT)): hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted(set(paths))}
frozen=inputs()
record={'scope':'CM0 recorded feasibility and implemented CM1 rectangles/slots/conics/splines/relations/Pierce, CM2 straight-line trim/extend and the CM3 driving/reference/component-dimension editor slice on the available Mac; other CM2 methods, external-geometry operands and full milestone acceptance remain open',
        'status':'In progress','steps':[], 'host':{'platform':platform.platform(), 'machine':platform.machine()},
        'native_requested':a.native,'blocked':['Full-round/general face blending not established',
            'Projected-edge/external-vertex CM1 operands depend on CM4; broader per-variant failure/resource acceptance, remaining CM2 editing modes, other CM3 dimensions/inference/drag and CM4-CM9 remain open',
            'Full physical 8 GB, installed, other-platform, physical-device and assistive-technology acceptance not run'],
        'preserved_deferred_gates':['6.1b','6.4c','7.4c'],'module_11_started':False}
if not a.native:
    record['blocked'].append('Native activation/scaling checks not run by this invocation; offscreen UI tests do not close native acceptance')
source_folder=Path(json.loads((ROOT/'docs/research/solidworks-training-sources.json').read_text())['source_folder'])
source_command=[sys.executable,'tools/check_core_modeling.py','--sources',str(source_folder)]
source_check=subprocess.run(source_command,cwd=ROOT,capture_output=True,text=True,timeout=60)
(folder/'original-sources.txt').write_text(source_check.stdout+source_check.stderr)
record['original_source_revalidation']={'status':'Pass' if source_check.returncode==0 else 'Blocked',
    'scope':'Optional original T2-T8 PDF hash validation; unavailable originals do not stop independent CAD checks',
    'command':source_command,'exit_code':source_check.returncode}
if source_check.returncode:
    record['blocked'].append('Original PDF revalidation unavailable or failed; see original-sources.txt')
record['host']['physical_ram_bytes']=int(subprocess.check_output(['sysctl','-n','hw.memsize'],text=True))
record['host']['cpu']=subprocess.check_output(['sysctl','-n','machdep.cpu.brand_string'],text=True).strip()
identity={'branch':subprocess.check_output(['git','branch','--show-current'],cwd=ROOT,text=True).strip(),
          'head':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
          'sha256':hashlib.sha256(json.dumps(frozen,sort_keys=True).encode()).hexdigest(),'inputs':frozen}
(folder/'source-identity.json').write_text(json.dumps(identity,indent=2)+'\n')
def save(): (folder/'verification.json').write_text(json.dumps(record,indent=2)+'\n')
def run(name,command,env=None,timeout=1800):
    print('Running '+name,flush=True);start=time.monotonic()
    with (folder/(name+'.txt')).open('w') as out:
        try: code=subprocess.run(command,cwd=ROOT,env=env,stdout=out,stderr=subprocess.STDOUT,timeout=timeout).returncode
        except subprocess.TimeoutExpired: code=124
    record['steps'].append({'name':name,'command':command,'exit_code':code,'seconds':time.monotonic()-start});save()
    if code:
        record['status']='Fail';save();raise SystemExit(f'Fail: {name}; see {folder}')
save()
run('configure',['cmake','--preset','macos-debug'])
run('build',['cmake','--build','--preset','macos-debug'])
print('Running isolated CM0 feasibility',flush=True)
probe=subprocess.run([str(ROOT/'build/macos-debug/cm0_feasibility')],capture_output=True,text=True,timeout=90)
(folder/'feasibility.json').write_text(probe.stdout)
(folder/'feasibility-stderr.txt').write_text(probe.stderr)
results=json.loads(probe.stdout)
assert len(results['experiments']) == 6
assert results['blocked_count'] == sum(r['status']=='Blocked' for r in results['experiments'])
assert all(r['status'] in {'Pass','Blocked'} for r in results['experiments'])
assert all(r.get('reason') for r in results['experiments'] if r['status']=='Blocked')
assert probe.returncode == (1 if results['blocked_count'] else 0)
record['feasibility']={'command':['build/macos-debug/cm0_feasibility'],'exit_code':probe.returncode,'results':results,
                       'meaning':'Probe failures remain Blocked; successful seeds are not production feature acceptance'};save()
run('ctest',['ctest','--preset','macos-debug','--output-on-failure'])
run('format',[sys.executable,'tools/format_sources.py'])
run('static-analysis',[sys.executable,'-u','tools/static_analysis.py','build/macos-debug/compile_commands.json'])
run('architecture',[sys.executable,'tools/check_architecture.py','build/macos-debug','--output',str(folder/'architecture.json')])
run('research-isolation',[sys.executable,'tools/check_isolation.py'])
run('coverage',[sys.executable,'tools/check_core_modeling.py'])
run('specifications',[sys.executable,'docs/scripts/check_specs.py'])
if a.native:
    env=os.environ.copy();env.pop('QT_QPA_PLATFORM',None);env['CAD_NATIVE_TESTS']='1'
    for percent,factor in [(100,'.5'),(125,'.625'),(150,'.75'),(175,'.875'),(200,None)]:
        out=folder/f'native-{percent}';out.mkdir();env['CAD_ARTIFACT_DIR']=str(out)
        env['CAD_EXPECTED_DPR']=str(percent/100)
        if factor: env['QT_SCALE_FACTOR']=factor
        else: env.pop('QT_SCALE_FACTOR',None)
        run(f'native-relations-{percent}',[str(ROOT/'build/macos-debug/cm1_contracts'),'nativeRelations'],env,180)
        run(f'native-rectangles-{percent}',[str(ROOT/'build/macos-debug/cm1_contracts'),'nativeRectangles'],env,240)
        for method in ['nativeSlotTools','nativeCurveTools','nativePierceTools','nativePointerPrimitives']:
            run(f'{method}-{percent}',[str(ROOT/'build/macos-debug/cm1_contracts'),method],env,240)
        run(f'native-dimensions-{percent}',[str(ROOT/'build/macos-debug/cm3_dimension_contracts'),'native'],env,240)
        run(f'native-line-editing-{percent}',[str(ROOT/'build/macos-debug/cm2_contracts'),'native'],env,240)
        if percent in [100,200]:
            run(f'native-workspace-{percent}',[str(ROOT/'build/macos-debug/module4_workspace'),'nativeWorkspace'],env,240)
            run(f'native-arcs-{percent}',[str(ROOT/'build/macos-debug/module3_contracts'),'nativeArcs'],env,180)
    env.pop('QT_SCALE_FACTOR',None)
if inputs()!=frozen:
    record['status']='Fail: source inputs changed during verification';save();raise SystemExit(record['status'])
record['status']='Pass: implemented CM1, CM2 straight-line editing and CM3 dimension slice verification; remaining editing/dimensions/inference/drag/external-geometry and full course/resource/platform gates remain open';save()
print(record['status'],flush=True)
