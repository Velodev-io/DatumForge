#!/usr/bin/env python3
"""Fresh source export and application build, with pinned source archives and existing built local dependencies.

This proves a clean application source build, not a second from-scratch dependency
build or a different platform. Keep those evidence classes separate.
"""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--without-geometry',action='store_true');p.add_argument('--module',choices=['0','1','2','3','4'],default='0');args=p.parse_args()
    subprocess.run(['python3',str(ROOT/'tools/fetch_dependencies.py')],check=True)
    files=subprocess.check_output(['git','ls-files','--cached','--others','--exclude-standard','-z'],cwd=ROOT).decode().split('\0')
    artifact=ROOT/f'docs/testing/artifacts/module-{args.module}/clean-reproduction.txt'
    with tempfile.TemporaryDirectory(prefix='cad-clean-source-') as directory,artifact.open('w') as log:
        clean=Path(directory)
        for name in filter(None,files):
            source=ROOT/name
            if source.is_file():
                dest=clean/name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,dest)
        (clean/'.deps').symlink_to(ROOT/'.deps',target_is_directory=True)
        def run(command):
            log.write('$ '+' '.join(command)+'\n');log.flush()
            subprocess.run(command,cwd=clean,stdout=log,stderr=subprocess.STDOUT,check=True)
        run(['git','init','-b','validation'])
        command=['cmake','--preset','macos-debug']
        if args.without_geometry:command+=['-DCAD_GEOMETRY_SPIKES=OFF']
        run(command);run(['cmake','--build','--preset','macos-debug'])
        run(['ctest','--preset','macos-debug','--output-on-failure'])
        log.write('Pass: fresh application source export/build/tests with SHA-256 verified source archives and existing local built libraries; no second dependency compilation and no installed-build claim.\n')
    print('Pass: clean source reproduction; see',artifact)

if __name__=='__main__':main()
