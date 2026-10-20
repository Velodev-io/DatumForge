#!/usr/bin/env python3
"""Inventory a ZIP SFX and inspect PE headers/imports without executing it."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import struct
import zipfile


def sha256(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def pe_info(path):
    data = path.read_bytes()
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    if data[pe:pe + 4] != b'PE\0\0':
        raise ValueError('Not a PE executable')
    machine, count = struct.unpack_from('<HH', data, pe + 4)
    optional_size = struct.unpack_from('<H', data, pe + 20)[0]
    optional = pe + 24
    magic = struct.unpack_from('<H', data, optional)[0]
    sections = []
    for index in range(count):
        pos = optional + optional_size + 40 * index
        virtual_size, rva, raw_size, raw_offset = struct.unpack_from('<IIII', data, pos + 8)
        sections.append(dict(name=data[pos:pos + 8].rstrip(b'\0').decode('ascii', 'replace'),
                             rva=rva, virtual_size=virtual_size, raw_size=raw_size, raw_offset=raw_offset))

    def offset(rva):
        for section in sections:
            delta = rva - section['rva']
            if 0 <= delta < section['raw_size']:
                return section['raw_offset'] + delta
        raise ValueError(f'Unmapped RVA {rva:x}')

    imports = []
    directory = optional + (112 if magic == 0x20b else 96)
    import_rva, import_size = struct.unpack_from('<II', data, directory + 8)
    if import_rva:
        start = offset(import_rva)
        for index in range(min(import_size // 20 + 1, 10000)):
            pos = start + 20 * index
            descriptor = struct.unpack_from('<IIIII', data, pos)
            if not any(descriptor):
                break
            name_pos = offset(descriptor[3])
            imports.append(data[name_pos:data.index(b'\0', name_pos)].decode('ascii', 'replace'))
    urls = sorted(set(m.decode('ascii', 'replace') for m in re.findall(rb'https?://[^\x00-\x20<>"\x7f-\xff]{4,}', data)))
    return dict(path=str(path), bytes=len(data), sha256=sha256(path), machine=hex(machine),
                optional_magic=hex(magic), sections=sections, imported_dlls=imports, embedded_ascii_urls=urls,
                limitation='Static headers/imports/strings only; strings do not establish executed behavior.')


def inspect(source, output):
    output.mkdir(parents=True, exist_ok=True)
    report = dict(source=pe_info(source), archive_entries=[], extracted=[])
    wanted = {'setup.exe', 'sldim/sldIM.exe', 'sldim/sldim.xml',
              'sldim/lang/english/html/imfuncs.js', 'sldim/lang/english/html/sldimWebView2.css'}
    with zipfile.ZipFile(source) as archive:
        for entry in archive.infolist():
            name = entry.filename.replace('\\', '/')
            report['archive_entries'].append(dict(path=name, bytes=entry.file_size, compressed_bytes=entry.compress_size,
                                                   crc32=f'{entry.CRC:08x}'))
            if name not in wanted:
                continue
            relative = PurePosixPath(name)
            if relative.is_absolute() or '..' in relative.parts:
                raise ValueError('Unsafe ZIP path')
            target = output / 'extracted' / Path(*relative.parts)
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(archive.read(entry))  # ZipFile verifies CRC on read.
            item = dict(path=name, bytes=target.stat().st_size, sha256=sha256(target), crc_verified=True)
            if name.lower().endswith('.exe'):
                item['pe'] = pe_info(target)
            report['extracted'].append(item)
    report['contains_sldworks_exe'] = any(PurePosixPath(e['path']).name.lower() == 'sldworks.exe' for e in report['archive_entries'])
    report['execution_performed'] = False
    destination = output / 'installer_inventory.json'
    destination.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(dict(report=str(destination), entries=len(report['archive_entries']),
                          extracted=len(report['extracted']), contains_sldworks_exe=report['contains_sldworks_exe'])))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    inspect(args.source, args.output)
