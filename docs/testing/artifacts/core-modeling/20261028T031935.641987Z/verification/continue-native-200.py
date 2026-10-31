"""Finish the unchanged-source 200% native matrix; retain the activation failure."""
import datetime
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time
ROOT = Path(__file__).resolve().parents[6]
FOLDER = Path(__file__).resolve().parent
identity = json.loads((FOLDER/'source-identity.json').read_text())
def inputs():
    paths = [ROOT/'CMakeLists.txt', ROOT/'CMakePresets.json', ROOT/'.clang-format', ROOT/'docs/scripts/check_specs.py']
    for top in ['apps','src','tests','tools','cmake']:
        paths += [f for f in (ROOT/top).rglob('*') if f.is_file() and f.suffix in {'.cpp','.hpp','.h','.py','.swift','.cmake','.json','.patch'}]
    return {str(f.relative_to(ROOT)): hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted(set(paths))}
record = {'scope':'Retry the existing curve-window activation failure and finish only the remaining 200% workflows; prior failure retained in verification.json', 'status':'In progress', 'original_run':'verification.json', 'started_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(), 'steps':[], 'source_identity':'source-identity.json', 'source_matches_before':inputs()==identity['inputs']}
report = FOLDER/'native-200-continuation.json'
def save(): report.write_text(json.dumps(record,indent=2)+'\n')
save()
if not record['source_matches_before']: raise SystemExit('Fail: source changed')
env = os.environ.copy()
for key in ['QT_QPA_PLATFORM','QT_SCALE_FACTOR']: env.pop(key,None)
env['CAD_NATIVE_TESTS']='1'
env['CAD_EXPECTED_DPR']='2.0'
out=FOLDER/'native-200-continuation'
out.mkdir(exist_ok=True)
env['CAD_ARTIFACT_DIR']=str(out)
checks=[('nativeCurveTools-200-retry','cm1_contracts','nativeCurveTools'),('nativePierceTools-200','cm1_contracts','nativePierceTools'),('nativePointerPrimitives-200','cm1_contracts','nativePointerPrimitives'),('native-dimensions-200','cm3_dimension_contracts','native'),('native-line-editing-200','cm2_contracts','native'),('native-workspace-200','module4_workspace','nativeWorkspace'),('native-arcs-200','module3_contracts','nativeArcs')]
for name, binary, method in checks:
    command=[str(ROOT/'build/macos-debug'/binary),method]
    print('Running '+name,flush=True)
    started=time.monotonic()
    with (FOLDER/(name+'-continuation.txt')).open('w') as log:
        try: code=subprocess.run(command,cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=240).returncode
        except subprocess.TimeoutExpired: code=124
    record['steps'].append({'name':name,'command':command,'exit_code':code,'seconds':time.monotonic()-started})
    save()
    if code:
        record['status']='Fail';save();raise SystemExit('Fail: '+name)
record['source_matches_after']=inputs()==identity['inputs']
record['status']='Pass: remaining 200% native checks and unchanged source verified; original activation failure retained' if record['source_matches_after'] else 'Fail: source changed'
save()
if not record['source_matches_after']: raise SystemExit(record['status'])
print(record['status'],flush=True)
