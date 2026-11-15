import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
ROOT=Path(__file__).resolve().parents[6]
FOLDER=Path(__file__).resolve().parent
old=json.loads((FOLDER/'source-identity.json').read_text())
def inputs():
    paths=[ROOT/'CMakeLists.txt',ROOT/'CMakePresets.json',ROOT/'.clang-format',ROOT/'docs/scripts/check_specs.py']
    for top in ['apps','src','tests','tools','cmake']:
        paths += [p for p in (ROOT/top).rglob('*') if p.is_file() and p.suffix in {'.cpp','.hpp','.h','.py','.swift','.cmake','.json','.patch'}]
    return {str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(set(paths))}
frozen=inputs()
changed=[p for p in set(frozen)|set(old['inputs']) if frozen.get(p)!=old['inputs'].get(p)]
assert changed==['tests/core_modeling/dimension_contracts.cpp'],changed
identity={'sha256':hashlib.sha256(json.dumps(frozen,sort_keys=True).encode()).hexdigest(),'inputs':frozen,'delta_from_full_suite':changed,'reason':'Test-only wait for paint-produced hit geometry; modal timer armed immediately before click. Original functional assertions retained.'}
(FOLDER/'source-identity-after-test-sync.json').write_text(json.dumps(identity,indent=2)+'\n')
preflight=Path('/private/tmp/cm3-native-repaint-20261029')
shutil.copytree(preflight,FOLDER/'native-100-after-test-sync',dirs_exist_ok=True)
assert 'Totals: 3 passed, 0 failed, 0 skipped' in (preflight/'result.txt').read_text()
record={'status':'In progress','scope':'Complete verification after a native-harness repaint synchronization correction; production inputs exactly match the passing full suite','source_identity':'source-identity-after-test-sync.json','delta_from_full_suite':changed,'steps':[],'preflight_native_100':'native-100-after-test-sync/result.txt'}
def save(): (FOLDER/'verification-after-test-sync.json').write_text(json.dumps(record,indent=2)+'\n')
def run(name,command,env=None):
    print('Running '+name,flush=True);start=time.monotonic()
    with (FOLDER/(name+'-after-test-sync.txt')).open('w') as log:
        try: code=subprocess.run(command,cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=300).returncode
        except subprocess.TimeoutExpired: code=124
    record['steps'].append({'name':name,'command':command,'exit_code':code,'seconds':time.monotonic()-start});save()
    return code
def require(name,command,env=None):
    if run(name,command,env):
        record['status']='Fail';save();raise SystemExit(name)
def native(name,binary,method,env):
    command=[str(ROOT/'build/macos-debug'/binary),method]
    if run(name,command,env):
        lines=(FOLDER/(name+'-after-test-sync.txt')).read_text().splitlines()
        failures=[line for line in lines if line.startswith('FAIL!')]
        if not failures or not all('QTest::qWaitForWindowActive(' in s and 'returned FALSE' in s for s in failures):
            record['status']='Fail';save();raise SystemExit(name)
        record.setdefault('activation_retries',[]).append(name);save()
        require(name+'-activation-retry',command,env)
save()
require('cm3-widget-repeat',['ctest','--preset','macos-debug','-R','^cm3\.dimension_contracts$','--repeat','until-fail:4','--output-on-failure'])
require('format',[sys.executable,'tools/format_sources.py'])
entries=json.loads((ROOT/'build/macos-debug/compile_commands.json').read_text())
filtered=[e for e in entries if str(Path(e['file']).resolve().relative_to(ROOT))==changed[0]]
assert len(filtered)==1
(FOLDER/'test-analysis-commands.json').write_text(json.dumps(filtered,indent=2)+'\n')
require('changed-test-analysis',[sys.executable,'-u','tools/static_analysis.py',str(FOLDER/'test-analysis-commands.json')])
env=os.environ.copy();env.pop('QT_QPA_PLATFORM',None);env['CAD_NATIVE_TESTS']='1'
for percent,factor in [(100,'.5'),(125,'.625'),(150,'.75'),(175,'.875'),(200,None)]:
    out=FOLDER/f'native-{percent}-after-test-sync';out.mkdir(exist_ok=True);env['CAD_ARTIFACT_DIR']=str(out)
    env['CAD_EXPECTED_DPR']=str(percent/100)
    if factor: env['QT_SCALE_FACTOR']=factor
    else: env.pop('QT_SCALE_FACTOR',None)
    if percent!=100: native(f'cm3-native-{percent}','cm3_dimension_contracts','native',env)
    if percent in [100,200]:
        native(f'workspace-native-{percent}','module4_workspace','nativeWorkspace',env)
        native(f'arcs-native-{percent}','module3_contracts','nativeArcs',env)
record['source_unchanged']=inputs()==frozen
record['status']='Pass: test synchronization verified; full-suite production source unchanged; native matrix complete' if record['source_unchanged'] else 'Fail: source changed'
save()
print(record['status'],flush=True)
if not record['source_unchanged']: raise SystemExit(1)
