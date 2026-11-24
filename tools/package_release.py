#!/usr/bin/env python3
"""Package native macOS arm64 standalone distribution bundle and disk image (Module 12)."""
import argparse
import hashlib
import json
import os
import plistlib
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEPS_LIB = ROOT / ".deps/install/lib"
DEPS_PLUGINS = ROOT / ".deps/install/plugins"
DEPS_SRC = ROOT / ".deps/src"

def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        while chunk := f.read(65536):
            h.update(chunk)
    return h.hexdigest()

def get_rpath_deps(path: Path) -> list[str]:
    out = subprocess.check_output(["otool", "-L", str(path)], text=True)
    deps = []
    for line in out.splitlines()[1:]:
        m = re.match(r"\s+(@rpath/[^\s]+)", line)
        if m:
            deps.append(m.group(1).replace("@rpath/", ""))
    return deps

def fix_rpaths(bin_path: Path, new_rpaths: list[str]):
    out = subprocess.check_output(["otool", "-l", str(bin_path)], text=True)
    lines = out.splitlines()
    for i, line in enumerate(lines):
        if "cmd LC_RPATH" in line:
            for j in range(i + 1, min(i + 5, len(lines))):
                if "path " in lines[j]:
                    old_path = lines[j].split("path ")[1].split(" (offset")[0].strip()
                    subprocess.run(["install_name_tool", "-delete_rpath", old_path, str(bin_path)], capture_output=True)
    for nr in new_rpaths:
        subprocess.run(["install_name_tool", "-add_rpath", nr, str(bin_path)], check=True, capture_output=True)

def codesign_file(path: Path):
    subprocess.run(["codesign", "--force", "--sign", "-", "--timestamp=none", str(path)], check=True, capture_output=True)

def package(build_dir: Path, output_dir: Path, version: str = "0.1.0", build_id: str = "20261123-arm64-release") -> dict:
    print(f"[package_release] Starting packaging from {build_dir} to {output_dir}")
    output_dir.mkdir(parents=True, exist_ok=True)
    staging_dir = output_dir / "staging"
    if staging_dir.exists():
        shutil.rmtree(staging_dir)
    staging_dir.mkdir(parents=True)

    src_app = build_dir / "cad-desktop.app"
    if not src_app.exists():
        raise FileNotFoundError(f"Source bundle not found at {src_app}")

    app_dir = staging_dir / "DatumForge.app"
    contents = app_dir / "Contents"
    macos = contents / "MacOS"
    frameworks = contents / "Frameworks"
    plugins = contents / "PlugIns"
    resources = contents / "Resources"

    macos.mkdir(parents=True)
    frameworks.mkdir(parents=True)
    plugins.mkdir(parents=True)
    resources.mkdir(parents=True)

    # 1. Copy binaries and strip debug symbols
    desktop_bin = src_app / "Contents/MacOS/cad-desktop"
    worker_bin = src_app / "Contents/MacOS/cad-worker"
    if not worker_bin.exists():
        worker_bin = build_dir / "cad-worker"

    dest_desktop = macos / "DatumForge"
    dest_worker = macos / "cad-worker"
    shutil.copy2(desktop_bin, dest_desktop)
    shutil.copy2(worker_bin, dest_worker)
    (macos / "cad-desktop").symlink_to("DatumForge")

    subprocess.run(["strip", "-x", str(dest_desktop)], check=True)
    subprocess.run(["strip", "-x", str(dest_worker)], check=True)

    # 2. Gather transitive runtime dylibs
    print("[package_release] Inventorying runtime shared libraries...")
    all_deps = set()
    for bin_path in [dest_desktop, dest_worker]:
        for d in get_rpath_deps(bin_path):
            all_deps.add(d)

    for p in DEPS_PLUGINS.rglob("*.dylib"):
        for d in get_rpath_deps(p):
            all_deps.add(d)

    changed = True
    while changed:
        changed = False
        for dep in list(all_deps):
            p = DEPS_LIB / dep if (DEPS_LIB / dep).exists() else (build_dir / dep)
            if p.exists():
                for subdep in get_rpath_deps(p):
                    if subdep not in all_deps:
                        all_deps.add(subdep)
                        changed = True

    print(f"[package_release] Bundling {len(all_deps)} shared libraries into Contents/Frameworks")
    for dep in sorted(all_deps):
        src_file = DEPS_LIB / dep if (DEPS_LIB / dep).exists() else (build_dir / dep)
        if src_file.is_symlink():
            target = src_file.resolve()
            dest_target = frameworks / target.name
            if not dest_target.exists():
                shutil.copy2(target, dest_target)
            link_path = frameworks / dep
            if not link_path.exists():
                link_path.symlink_to(target.name)
        elif src_file.exists():
            dest_file = frameworks / dep
            if not dest_file.exists():
                shutil.copy2(src_file, dest_file)

    # Symlink Contents/lib -> Frameworks for plugins using @loader_path/../../lib
    (contents / "lib").symlink_to("Frameworks")

    # 3. Bundle Qt plugins
    print("[package_release] Bundling Qt plugins (platforms, styles, imageformats)...")
    for plugin_dir in ["platforms", "styles", "imageformats"]:
        src_pdir = DEPS_PLUGINS / plugin_dir
        if src_pdir.exists():
            dest_pdir = plugins / plugin_dir
            if dest_pdir.exists():
                shutil.rmtree(dest_pdir)
            shutil.copytree(src_pdir, dest_pdir)

    # 4. Create qt.conf
    (resources / "qt.conf").write_text(
        "[Paths]\nPrefix = ..\nPlugins = PlugIns\nLibraries = Frameworks\n"
    )

    # 5. Create Info.plist
    plist_data = {
        "CFBundleDevelopmentRegion": "en",
        "CFBundleDisplayName": "DatumForge",
        "CFBundleExecutable": "DatumForge",
        "CFBundleIdentifier": "com.datumforge.desktop",
        "CFBundleInfoDictionaryVersion": "6.0",
        "CFBundleName": "DatumForge",
        "CFBundlePackageType": "APPL",
        "CFBundleShortVersionString": version,
        "CFBundleSignature": "DFRG",
        "CFBundleVersion": version,
        "LSMinimumSystemVersion": "13.0",
        "NSHighResolutionCapable": True,
        "NSPrincipalClass": "NSApplication",
        "NSHumanReadableCopyright": "Copyright © 2026 DatumForge Authors. All rights reserved.",
    }
    with (contents / "Info.plist").open("wb") as f:
        plistlib.dump(plist_data, f)
    (contents / "PkgInfo").write_bytes(b"APPLDFRG")

    # 6. Bundle Licenses and Notices
    print("[package_release] Bundling third-party licenses and legal notices...")
    licenses_dir = resources / "LICENSES"
    licenses_dir.mkdir(parents=True, exist_ok=True)

    # Pinned license sources
    if (DEPS_SRC / "occt/LICENSE_LGPL_21.txt").exists():
        shutil.copy2(DEPS_SRC / "occt/LICENSE_LGPL_21.txt", licenses_dir / "OCCT_LGPL_21.txt")
    if (DEPS_SRC / "occt/OCCT_LGPL_EXCEPTION.txt").exists():
        shutil.copy2(DEPS_SRC / "occt/OCCT_LGPL_EXCEPTION.txt", licenses_dir / "OCCT_LGPL_EXCEPTION.txt")
    if (DEPS_SRC / "eigen/COPYING.MPL2").exists():
        shutil.copy2(DEPS_SRC / "eigen/COPYING.MPL2", licenses_dir / "Eigen_MPL2.txt")
    if (DEPS_SRC / "boost/LICENSE_1_0.txt").exists():
        shutil.copy2(DEPS_SRC / "boost/LICENSE_1_0.txt", licenses_dir / "Boost_BSL_1_0.txt")
    if (DEPS_SRC / "ninja/COPYING").exists():
        shutil.copy2(DEPS_SRC / "ninja/COPYING", licenses_dir / "Ninja_Apache_2_0.txt")

    qt_licenses_src = DEPS_SRC / "qtbase/LICENSES"
    if qt_licenses_src.exists():
        qt_dest = licenses_dir / "Qt6"
        if qt_dest.exists():
            shutil.rmtree(qt_dest)
        shutil.copytree(qt_licenses_src, qt_dest)

    (licenses_dir / "PlaneGCS_LGPL_2_0.txt").write_text(
        "PlaneGCS 2D Geometric Constraint Solver\n"
        "Derived from FreeCAD (LGPL v2.1 or later)\n"
        "Copyright (C) FreeCAD Developers\n\n"
        "This library is free software; you can redistribute it and/or\n"
        "modify it under the terms of the GNU Library General Public\n"
        "License as published by the Free Software Foundation; either\n"
        "version 2 of the License, or (at your option) any later version.\n"
    )

    (licenses_dir / "SQLite_Public_Domain.txt").write_text(
        "SQLite 3.50.4\n"
        "2001-09-15\n\n"
        "The author disclaims copyright to this source code. In place of\n"
        "a legal notice, here is a blessing:\n\n"
        "  May you do good and not evil.\n"
        "  May you find forgiveness for yourself and forgive others.\n"
        "  May you share freely, never taking more than you give.\n"
    )

    notices_text = f"""DatumForge — Third-Party Notices and Legal Attributions
Version: {version}
Build ID: {build_id}

This standalone distribution of DatumForge bundles dynamically linked shared
libraries and components subject to third-party open-source licenses:

1. Open CASCADE Technology (OCCT) 7.9.2
   License: GNU Lesser General Public License version 2.1 (LGPL-2.1) with OCCT Exception.
   Dynamic shared libraries: libTKernel, libTKMath, libTKBRep, libTKTopAlgo, libTKPrim,
   libTKBO, libTKBool, libTKFillet, libTKMesh, libTKDESTEP, libTKDEIGES, libTKDESTL, etc.
   Source availability: Corresponding upstream source available from https://dev.opencascade.org

2. Qt 6.10.0 (Core, Gui, Widgets, Network, OpenGL, OpenGLWidgets, Cocoa platform plugin)
   License: GNU Lesser General Public License version 3.0 (LGPL-3.0).
   Dynamic shared libraries: libQt6Core, libQt6Gui, libQt6Widgets, libQt6Network, etc.
   Relinking Rights: Per LGPL-3.0 §4(d)(1), users may replace or relink bundled Qt shared
   libraries with compatible modified versions located in Contents/Frameworks/ without
   modifying DatumForge application source.

3. PlaneGCS (FreeCAD 1.0.2 revision)
   License: GNU Library General Public License version 2 or later (LGPL-2.0+).
   Dynamic shared library: libcad_planegcs.dylib in Contents/Frameworks/.
   Role: 2D geometric constraint solving engine (user-relinkable dynamic library).

4. Eigen 3.4.0
   License: Mozilla Public License version 2.0 (MPL-2.0).
   Role: Matrix algebra and spatial transforms.

5. Boost 1.86.0
   License: Boost Software License 1.0 (BSL-1.0).

6. SQLite 3.50.4
   License: Public Domain.
   Role: Atomic transactional project store and document revisions.

Full license texts are preserved under Contents/Resources/LICENSES/.
"""
    (resources / "NOTICES.txt").write_text(notices_text)

    # 7. Create version-manifest.json
    manifest = {
        "product_name": "DatumForge",
        "application_id": "com.datumforge.desktop",
        "version": version,
        "build_id": build_id,
        "target_os": "macOS",
        "target_architecture": "arm64",
        "minimum_system_version": "13.0",
        "schema_version": 4,
        "kernel": "OpenCASCADE 7.9.2",
        "gui_toolkit": "Qt 6.10.0",
        "solver": "PlaneGCS (FreeCAD 1.0.2 pinned revision)",
        "storage": "SQLite 3.50.4",
        "distribution_license": "Proprietary Commercial / Source-Available Evaluation",
        "code_signing_policy": "ad-hoc (developer ID unprovisioned)",
        "network_dependency": "none (100% offline, zero external network dependency)",
        "debug_endpoints": False,
        "bundled_libraries_count": len(all_deps),
    }
    (resources / "version-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")

    # 8. Adjust rpaths on binaries, frameworks, and plugins
    print("[package_release] Adjusting Mach-O LC_RPATH load commands...")
    fix_rpaths(dest_desktop, ["@executable_path/../Frameworks", "@loader_path/../Frameworks"])
    fix_rpaths(dest_worker, ["@executable_path/../Frameworks", "@loader_path/../Frameworks"])
    planegcs_dylib = frameworks / "libcad_planegcs.dylib"
    if planegcs_dylib.exists() and not planegcs_dylib.is_symlink():
        subprocess.run(
            ["install_name_tool", "-id", "@rpath/libcad_planegcs.dylib", str(planegcs_dylib)],
            check=True,
            capture_output=True,
        )
        fix_rpaths(planegcs_dylib, ["@loader_path", "@executable_path/../Frameworks"])
    for p in plugins.rglob("*.dylib"):
        fix_rpaths(p, ["@loader_path/../../Frameworks", "@executable_path/../Frameworks"])

    # 9. Codesign all components inside out
    print("[package_release] Applying ad-hoc codesign to bundle components...")
    for p in frameworks.glob("*.dylib"):
        if not p.is_symlink():
            codesign_file(p)
    for p in plugins.rglob("*.dylib"):
        codesign_file(p)
    codesign_file(dest_worker)
    codesign_file(dest_desktop)
    codesign_file(app_dir)

    # Verify codesign
    subprocess.run(["codesign", "--verify", "--deep", "--strict", str(app_dir)], check=True)
    print("[package_release] Standalone bundle codesign verified: PASS")

    # 10. Create DMG disk image
    dmg_name = f"DatumForge-{version}-mac-arm64.dmg"
    dmg_path = output_dir / dmg_name
    print(f"[package_release] Creating disk image {dmg_path}...")
    dmg_stage = output_dir / "dmg_stage"
    if dmg_stage.exists():
        shutil.rmtree(dmg_stage)
    dmg_stage.mkdir(parents=True)

    # Copy app into dmg stage
    shutil.copytree(app_dir, dmg_stage / "DatumForge.app", symlinks=True)
    (dmg_stage / "Applications").symlink_to("/Applications")
    (dmg_stage / "README.txt").write_text(
        f"DatumForge {version} for macOS (Apple Silicon arm64)\n\n"
        "Installation Instructions:\n"
        "1. Drag 'DatumForge.app' into the 'Applications' shortcut.\n"
        "2. Open DatumForge from /Applications or Launchpad.\n"
        "3. Supported Operating Systems: macOS 13.0 (Ventura), macOS 14.0 (Sonoma),\n"
        "   macOS 15.0 (Sequoia), and macOS 26/27 on Apple Silicon (M1/M2/M3/M4).\n"
        "4. 100% Offline: No internet connection is ever required.\n\n"
        "See NOTICES.txt for third-party open-source licensing and relinking information.\n"
    )
    (dmg_stage / "NOTICES.txt").write_text(notices_text)
    (dmg_stage / "LICENSE.txt").write_text(
        "DatumForge End User License Agreement\n\n"
        "Copyright © 2026 DatumForge Authors.\n"
        "This software is provided for evaluation and authorized engineering use.\n"
        "Bundled third-party components remain subject to their respective open-source licenses.\n"
    )

    if dmg_path.exists():
        dmg_path.unlink()
    subprocess.run(
        ["hdiutil", "create", "-volname", "DatumForge", "-srcfolder", str(dmg_stage), "-ov", "-format", "UDZO", str(dmg_path)],
        check=True,
        capture_output=True
    )
    codesign_file(dmg_path)
    shutil.rmtree(dmg_stage)
    print(f"[package_release] Disk image created: {dmg_path} ({dmg_path.stat().st_size} bytes)")

    # 11. Create ZIP archive of the bundle
    zip_name = f"DatumForge-{version}-mac-arm64.zip"
    zip_path = output_dir / zip_name
    print(f"[package_release] Creating zip archive {zip_path}...")
    if zip_path.exists():
        zip_path.unlink()
    subprocess.run(["ditto", "-c", "-k", "--keepParent", str(app_dir), str(zip_path)], check=True)
    print(f"[package_release] Zip archive created: {zip_path} ({zip_path.stat().st_size} bytes)")

    # 12. Write package manifest with SHA-256 hashes
    result_manifest = {
        "version": version,
        "build_id": build_id,
        "target_os": "macOS",
        "target_architecture": "arm64",
        "artifacts": {
            "app_bundle": {
                "name": "DatumForge.app",
                "path": str(app_dir.relative_to(ROOT)),
                "codesign_verified": True,
            },
            "dmg": {
                "name": dmg_name,
                "path": str(dmg_path.relative_to(ROOT)),
                "size_bytes": dmg_path.stat().st_size,
                "sha256": sha256_file(dmg_path),
            },
            "zip": {
                "name": zip_name,
                "path": str(zip_path.relative_to(ROOT)),
                "size_bytes": zip_path.stat().st_size,
                "sha256": sha256_file(zip_path),
            },
        },
        "contents": manifest,
    }
    manifest_file = output_dir / "package-manifest.json"
    manifest_file.write_text(json.dumps(result_manifest, indent=2) + "\n")
    print(f"[package_release] Package manifest recorded at {manifest_file}")
    return result_manifest

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Package native DatumForge macOS release")
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build/macos-release")
    parser.add_argument("--output-dir", type=Path, default=ROOT / "out/packages")
    parser.add_argument("--version", default="0.1.0")
    parser.add_argument("--build-id", default="20261123-arm64-release")
    args = parser.parse_args()
    package(args.build_dir, args.output_dir, args.version, args.build_id)
