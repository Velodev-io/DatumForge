#!/usr/bin/env python3
"""Inventory exact source license/attribution texts without publishing a package."""
import hashlib
import json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
base=ROOT/'.deps/src'
files=[]
files.extend((base/'qtbase/LICENSES').glob('*.txt'))
files.extend((base/'qtbase/src/3rdparty').rglob('qt_attribution.json'))
for name in ['occt/LICENSE_LGPL_21.txt','occt/OCCT_LGPL_EXCEPTION.txt','eigen/COPYING.MPL2',
             'eigen/COPYING.README','boost/LICENSE_1_0.txt','ninja/COPYING','sqlite/sqlite3.c']:
    files.append(base/name)
files.extend((base/'planegcs').glob('*'))
output={str(p.relative_to(base)):{'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'bytes':p.stat().st_size}
        for p in sorted(files)}
(ROOT/'docs/testing/artifacts/module-0/source-notices.json').write_text(json.dumps(output,indent=2)+'\n')
print(f'Inventoried {len(output)} exact license/attribution/source notice files; release compliance review pending')
