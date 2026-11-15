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
identity=json.loads((FOLDER/'source-identity-after-test-sync.json').read_text())
def inputs():
    paths=[ROOT/'CMakeLists.txt',ROOT/'CMakePresets.json',ROOT/'.clang-format',ROOT/'docs/scripts/check_specs.py']
    for top in ['apps','src','tests','tools','cmake']:
        paths += [p for p in (ROOT/top).rglob('*') if p.is_file() and p.suffix in {'.cpp','.hpp','.h','.py','.swift','.cmake','.json','.patch'}]
    return {str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(set(paths))}
record={'status':'In progress','source_matches_before':inputs()==identity['inputs'],'source_identity':'source-identity-after-test-sync.json','steps':[], 'retained_failure':'arcs-native-100-after-test-sync.txt: one tangent instead of two; no production change; isolated rerun passed; cause unproven'}
assert record['source_matches_before']
shutil.copytree('/private/tmp/cm3-arcs-recheck-20261029',FOLDER/'arcs-native-100-isolated-recheck',dirs_exist_ok=True)
assert 'Totals: 3 passed, 0 failed, 0 skipped' in (FOLDER/'arcs-native-100-isolated-recheck/result.txt').read_text()
def save(): (FOLDER/'native-matrix-completion.json').write_text(json.dumps(record,indent=2)+'\n')
def check(name,binary,method,env):
    command=[str(ROOT/'build/macos-debug'/binary),method]
    print('Running '+name,flush=True);start=time.monotonic()
    with (FOLDER/(name+'.txt')).open('w') as log:
        try: code=subprocess.run(command,cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=240).returncode
        except subprocess.TimeoutExpired: code=124
    record['steps'].append({'name':name,'command':command,'exit_code':code,'seconds':time.monotonic()-start});save()
    if code:record['status']='Fail';save();raise SystemExit(name)
env=os.environ.copy();env.pop('QT_QPA_PLATFORM',None);env['CAD_NATIVE_TESTS']='1'
env['CAD_EXPECTED_DPR']='1';env['QT_SCALE_FACTOR']='.5'
for i in [2,3]:
    out=FOLDER/f'arc-100-recheck-{i}';out.mkdir();env['CAD_ARTIFACT_DIR']=str(out)
    check(f'arc-100-recheck-{i}','module3_contracts','nativeArcs',env)
for percent,factor in [(125,'.625'),(150,'.75'),(175,'.875'),(200,None)]:
    out=FOLDER/f'native-{percent}-completion';out.mkdir();env['CAD_ARTIFACT_DIR']=str(out);env['CAD_EXPECTED_DPR']=str(percent/100)
    if factor:env['QT_SCALE_FACTOR']=factor
    else:env.pop('QT_SCALE_FACTOR',None)
    check(f'cm3-native-{percent}-completion','cm3_dimension_contracts','native',env)
    if percent==200:
        check('workspace-native-200-completion','module4_workspace','nativeWorkspace',env)
        check('arcs-native-200-completion','module3_contracts','nativeArcs',env)
record['source_matches_after']=inputs()==identity['inputs']
record['status']='Pass: native matrix complete; isolated legacy arc-count failure retained, cause not established' if record['source_matches_after'] else 'Fail: source changed'
save();print(record['status'],flush=True)
if not record['source_matches_after']:raise SystemExit(1)
