import hashlib
import json
from pathlib import Path
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[6]
FOLDER=Path(__file__).resolve().parent
identity=json.loads((FOLDER/'source-identity.json').read_text())
def inputs():
    paths=[ROOT/'CMakeLists.txt',ROOT/'CMakePresets.json',ROOT/'.clang-format',ROOT/'docs/scripts/check_specs.py']
    for top in ['apps','src','tests','tools','cmake']:
        paths += [p for p in (ROOT/top).rglob('*') if p.is_file() and p.suffix in {'.cpp','.hpp','.h','.py','.swift','.cmake','.json','.patch'}]
    return {str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(set(paths))}
report={'status':'In progress','scope':'Static continuation only; failed full suite and unfinished native checks are retained separately','steps':[],'source_matches_before':inputs()==identity['inputs']}
def save(): (FOLDER/'static-continuation.json').write_text(json.dumps(report,indent=2)+'\n')
save()
assert report['source_matches_before']
selected={'src/sketch/Sketch.cpp','src/sketch/Dimensions.cpp','src/sketch/Solver.cpp','src/persistence/ProjectStore.cpp','apps/desktop/DimensionUI.cpp','apps/desktop/SketchPanel.cpp','src/render/SketchView.cpp','tests/core_modeling/associated_dimensions.cpp','tests/core_modeling/dimensions.cpp','tests/core_modeling/dimension_contracts.cpp'}
entries=json.loads((ROOT/'build/macos-debug/compile_commands.json').read_text())
filtered=[e for e in entries if str(Path(e['file']).resolve().relative_to(ROOT)) in selected]
assert {str(Path(e['file']).resolve().relative_to(ROOT)) for e in filtered}==selected
(FOLDER/'analysis-commands.json').write_text(json.dumps(filtered,indent=2)+'\n')
checks=[('format',[sys.executable,'tools/format_sources.py']),('changed-source-analysis',[sys.executable,'-u','tools/static_analysis.py',str(FOLDER/'analysis-commands.json')]),('architecture',[sys.executable,'tools/check_architecture.py','build/macos-debug','--output',str(FOLDER/'architecture.json')]),('research-isolation',[sys.executable,'tools/check_isolation.py']),('coverage',[sys.executable,'tools/check_core_modeling.py']),('specifications',[sys.executable,'docs/scripts/check_specs.py'])]
for name,command in checks:
    print('Running '+name,flush=True)
    with (FOLDER/(name+'-continuation.txt')).open('w') as log:
        try: code=subprocess.run(command,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,timeout=1800).returncode
        except subprocess.TimeoutExpired: code=124
    report['steps'].append({'name':name,'command':command,'exit_code':code});save()
    if code:
        report['status']='Fail';save();raise SystemExit(name)
report['source_matches_after']=inputs()==identity['inputs']
report['status']='Pass: static checks only; full/native verification remains incomplete' if report['source_matches_after'] else 'Fail: source changed'
save()
print(report['status'],flush=True)
