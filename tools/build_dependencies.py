#!/usr/bin/env python3
"""Verify the committed source lock, then build into .deps only (no network)."""
import argparse
import hashlib
import json
import os
import platform
from pathlib import Path
import shutil
import subprocess
import tarfile
import zipfile
from contextlib import contextmanager

ROOT = Path(__file__).resolve().parents[1]
DEPS = ROOT / '.deps'

@contextmanager
def setup_lock():
    DEPS.mkdir(exist_ok=True)
    with (DEPS / '.setup.lock').open('a+b') as handle:
        if platform.system() == 'Windows':
            import msvcrt
            handle.seek(0); handle.write(b'0'); handle.flush(); handle.seek(0)
            msvcrt.locking(handle.fileno(), msvcrt.LK_NBLCK, 1)
        else:
            import fcntl
            fcntl.flock(handle, fcntl.LOCK_EX | fcntl.LOCK_NB)
        yield

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jobs', type=int, default=2)
    parser.add_argument('--only', choices=['ninja', 'qtbase', 'occt', 'all'], default='all')
    args = parser.parse_args()
    if not 1 <= args.jobs <= 4:
        parser.error('Use 1–4 jobs on the 8 GB development baseline')
    lock = json.loads((ROOT / 'cmake/dependencies.lock.json').read_text())
    source_root = DEPS / 'src'
    source_root.mkdir(parents=True, exist_ok=True)
    for name, entry in lock['sources'].items():
        archive = DEPS / 'downloads' / entry['file']
        if hashlib.sha256(archive.read_bytes()).hexdigest() != entry['sha256']:
            raise RuntimeError('Source checksum mismatch: ' + name)
        if name.startswith('planegcs/'):
            dest = source_root / name
            dest.parent.mkdir(exist_ok=True)
            shutil.copyfile(archive, dest)
            continue
        dest = source_root / name
        if dest.exists():
            continue
        temp = source_root / (name + '-unpack')
        temp.mkdir(exist_ok=True)
        if archive.suffix == '.zip':
            with zipfile.ZipFile(archive) as z:
                for info in z.infolist():
                    if not (temp / info.filename).resolve().is_relative_to(temp.resolve()):
                        raise RuntimeError('Unsafe archive path')
                z.extractall(temp)
        else:
            with tarfile.open(archive) as t:
                t.extractall(temp, filter='data')
        items = list(temp.iterdir())
        if len(items) != 1 or not items[0].is_dir():
            raise RuntimeError('Unexpected source layout: ' + name)
        items[0].rename(dest)
        temp.rmdir()
    logs = DEPS / 'logs'
    logs.mkdir(exist_ok=True)
    prefix = DEPS / 'install'
    env = os.environ.copy()
    env['PATH'] = str(prefix / 'bin') + os.pathsep + env['PATH']
    def run(command, name):
        print(name + ': ' + ' '.join(map(str, command)), flush=True)
        with (logs / (name + '.txt')).open('w') as log:
            result = subprocess.run(list(map(str, command)), env=env, stdout=log, stderr=subprocess.STDOUT)
        if result.returncode:
            print((logs / (name + '.txt')).read_text()[-6000:])
            raise SystemExit(result.returncode)
    def build(name, options, generator='Ninja'):
        directory = DEPS / 'build' / name
        run(['cmake', '-S', source_root / name, '-B', directory, '-G', generator,
             '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_INSTALL_PREFIX=' + str(prefix), *options], name + '-configure')
        run(['cmake', '--build', directory, '--parallel', str(args.jobs)], name + '-build')
        run(['cmake', '--install', directory], name + '-install')
    if args.only in ('ninja', 'all'):
        build('ninja', ['-DBUILD_TESTING=OFF'], 'NMake Makefiles' if platform.system() == 'Windows' else 'Unix Makefiles')
    common = ['-DCMAKE_OSX_DEPLOYMENT_TARGET=13.0'] if platform.system() == 'Darwin' else []
    if args.only in ('qtbase', 'all'):
        patch = ROOT / 'cmake/patches/qt-6.10.0-arm-yield.patch'
        helper = source_root / 'qtbase/src/corelib/thread/qyieldcpu.h'
        if '#  include <arm_acle.h>' not in helper.read_text():
            with patch.open('r') as content:
                subprocess.run(['patch', '-p1'], cwd=source_root / 'qtbase', stdin=content, check=True)
        build('qtbase', common + ['-DQT_BUILD_TESTS=OFF', '-DQT_BUILD_EXAMPLES=OFF',
            '-DFEATURE_framework=OFF', '-DFEATURE_dbus=OFF', '-DFEATURE_sql=OFF',
            '-DFEATURE_openssl=OFF', '-DFEATURE_icu=OFF'])
    if args.only in ('occt', 'all'):
        optimization = '/O1 /DNDEBUG' if platform.system() == 'Windows' else '-O1 -DNDEBUG'
        build('occt', common + ['-DBUILD_MODULE_Draw=OFF', '-DBUILD_MODULE_Visualization=OFF',
            '-DBUILD_RELEASE_DISABLE_EXCEPTIONS=OFF', '-DCMAKE_CXX_FLAGS_RELEASE=' + optimization,
            '-DCMAKE_EXPORT_NO_PACKAGE_REGISTRY=ON',
            '-DUSE_TCL=OFF', '-DUSE_FREETYPE=OFF', '-DUSE_TK=OFF', '-DUSE_TBB=OFF',
            '-DUSE_VTK=OFF', '-DUSE_FFMPEG=OFF', '-DBUILD_LIBRARY_TYPE=Shared'])

if __name__ == '__main__':
    with setup_lock():
        main()
