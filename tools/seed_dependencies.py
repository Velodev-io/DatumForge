#!/usr/bin/env python3
"""Maintainer-only initial source selection. Never called by normal builds.

Writes a candidate lock from immutable upstream sources; compatibility results
must be reviewed before marking the lock verified. Network only at setup time.
"""
import concurrent.futures
import hashlib
import json
from pathlib import Path
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
DOWNLOAD = ROOT / '.deps/downloads'
SOURCES = {
    'qtbase': ('6.10.0', 'https://download.qt.io/official_releases/qt/6.10/6.10.0/submodules/qtbase-everywhere-src-6.10.0.tar.xz', 'qtbase.tar.xz'),
    'occt': ('7.9.2', 'https://codeload.github.com/Open-Cascade-SAS/OCCT/tar.gz/c5f20409c52bf8f658314d205a0e5d6f0be0969c', 'occt.tar.gz'),
    'eigen': ('3.4.0', 'https://gitlab.com/libeigen/eigen/-/archive/3147391d946bb4b6c68edd901f2add6ac1f31f8c/eigen-3147391d946bb4b6c68edd901f2add6ac1f31f8c.tar.gz', 'eigen.tar.gz'),
    'sqlite': ('3.50.4', 'https://www.sqlite.org/2025/sqlite-amalgamation-3500400.zip', 'sqlite.zip'),
    'ninja': ('1.13.1', 'https://codeload.github.com/ninja-build/ninja/tar.gz/refs/tags/v1.13.1', 'ninja.tar.gz'),
    'boost': ('1.86.0', 'https://archives.boost.io/release/1.86.0/source/boost_1_86_0.tar.bz2', 'boost.tar.bz2'),
}

def fetch(item):
    name, (version, url, filename) = item
    path = DOWNLOAD / filename
    if not path.exists():
        with urllib.request.urlopen(url, timeout=90) as source, path.with_suffix('.partial').open('wb') as out:
            while data := source.read(1024 * 1024):
                out.write(data)
        path.with_suffix('.partial').replace(path)
    return name, {'version': version, 'url': url, 'file': filename,
                  'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}

def main():
    DOWNLOAD.mkdir(parents=True, exist_ok=True)
    entries = list(SOURCES.items())
    revision = '256fc7eff3379911ab5daf88e10182c509aa8052'
    for file in json.loads((DOWNLOAD / 'planegcs-directory.json').read_text()):
        entries.append(('planegcs/' + file['name'], ('FreeCAD-1.0.2', f"https://raw.githubusercontent.com/FreeCAD/FreeCAD/{revision}/src/Mod/Sketcher/App/planegcs/{file['name']}", 'planegcs-' + file['name'])))
    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
        sources = dict(pool.map(fetch, entries))
    official_qt = (DOWNLOAD / 'qtbase.sha256').read_text().split()[0]
    if sources['qtbase']['sha256'] != official_qt:
        raise RuntimeError('Qt official checksum mismatch')
    lock = {'schema': 1, 'status': 'candidate; compatibility build pending',
            'strategy': 'project-local source builds; C++20 application; shared Qt/OCCT; no system release dependencies',
            'toolchain': {'cmake': '4.3.1', 'macos_compiler': 'AppleClang 21.0.0 (clang-2100.1.1.101)', 'ninja': '1.13.1', 'cxx_runtime': 'libc++ on Mac; platform presets pending verification'},
            'solver_revision': revision, 'occt_revision': 'c5f20409c52bf8f658314d205a0e5d6f0be0969c',
            'eigen_revision': '3147391d946bb4b6c68edd901f2add6ac1f31f8c',
            'sources': sources}
    (ROOT / 'cmake/dependencies.lock.json').write_text(json.dumps(lock, indent=2) + '\n')
    print('Candidate lock written:', len(sources), 'hashed inputs')

if __name__ == '__main__':
    main()
