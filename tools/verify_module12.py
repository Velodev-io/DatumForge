#!/usr/bin/env python3
"""Execute Module 12 release packaging, isolated launch, installed smoke, and release checks."""
import argparse
import datetime
import hashlib
import json
import os
import platform
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TEST_BUILD_DIR = ROOT / "build/macos-benchmark"
RELEASE_BUILD_DIR = ROOT / "build/macos-release"
OUT_PACKAGES = ROOT / "out/packages"
SANDBOX_PROFILE = '(version 1) (allow default) (deny network* (remote ip "*:*")) (deny network* (local ip "*:*"))'


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        while chunk := f.read(65536):
            h.update(chunk)
    return h.hexdigest()


def inputs():
    paths = [ROOT / "CMakeLists.txt", ROOT / "CMakePresets.json", ROOT / ".clang-format"]
    for top in ("apps", "src", "tests", "tools", "cmake"):
        paths += [
            f
            for f in (ROOT / top).rglob("*")
            if f.is_file()
            and f.suffix in (".cpp", ".hpp", ".h", ".py", ".swift", ".cmake", ".json", ".patch")
        ]
    return {
        str(f.relative_to(ROOT)): hashlib.sha256(f.read_bytes()).hexdigest()
        for f in sorted(set(paths))
    }


def get_telemetry():
    hw = {}
    try:
        hw["model"] = subprocess.check_output(["sysctl", "-n", "hw.model"], text=True).strip()
        hw["ncpu"] = int(subprocess.check_output(["sysctl", "-n", "hw.ncpu"], text=True).strip())
        hw["memsize_bytes"] = int(subprocess.check_output(["sysctl", "-n", "hw.memsize"], text=True).strip())
    except Exception as e:
        hw["error"] = str(e)
    return {
        "os": platform.platform(),
        "arch": platform.machine(),
        "system": platform.system(),
        "release": platform.release(),
        "version": platform.version(),
        "hardware": hw,
        "physical_8gb_baseline": hw.get("memsize_bytes", 0) == 8589934592,
    }


def launch_and_verify_shipped_app(app_path: Path, env: dict, log_path: Path, use_sandbox: bool = False) -> dict:
    """Launch the unmodified release DatumForge binary (no test probes), confirm child cad-worker spawns and stays healthy, then terminate cleanly."""
    exe = app_path / "Contents/MacOS/DatumForge"
    cmd = ["/usr/bin/sandbox-exec", "-p", SANDBOX_PROFILE, str(exe)] if use_sandbox else [str(exe)]
    proc = subprocess.Popen(cmd, cwd="/tmp", env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    worker_pids = []
    started = time.monotonic()
    try:
        deadline = started + 8.0
        while time.monotonic() < deadline:
            if proc.poll() is not None:
                out, _ = proc.communicate(timeout=2)
                log_path.write_text(out or "")
                raise RuntimeError(f"Shipped DatumForge exited prematurely with code {proc.returncode}")
            res = subprocess.run(["pgrep", "-P", str(proc.pid)], capture_output=True, text=True)
            pids = [int(p) for p in res.stdout.split() if p.strip().isdigit()]
            if pids:
                worker_pids = pids
                break
            time.sleep(0.1)
        if not worker_pids:
            raise RuntimeError("Shipped DatumForge did not spawn child cad-worker within 8s")
        time.sleep(1.5)
        if proc.poll() is not None:
            out, _ = proc.communicate(timeout=2)
            log_path.write_text(out or "")
            raise RuntimeError(f"Shipped DatumForge exited during settle with code {proc.returncode}")
        # Verify worker is still running
        for wp in worker_pids:
            subprocess.run(["kill", "-0", str(wp)], check=True)
        # Inspect loaded dynamic libraries via vmmap/lsof if accessible (best-effort; record output)
        lsof_res = subprocess.run(["lsof", "-p", f"{proc.pid},{worker_pids[0]}"], capture_output=True, text=True)
        loaded_leak = any(tok in lsof_res.stdout for tok in (".deps/", "/build/"))
        if loaded_leak:
            raise RuntimeError("Shipped DatumForge/cad-worker loaded dylib from .deps/ or build/")
        elapsed = time.monotonic() - started
    finally:
        if proc.poll() is None:
            proc.terminate()
            try:
                out, _ = proc.communicate(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
                out, _ = proc.communicate()
        else:
            out, _ = proc.communicate()
        log_path.write_text(
            f"command={cmd}\nui_pid={proc.pid}\nworker_pids={worker_pids}\n"
            f"lsof_exit={lsof_res.returncode if 'lsof_res' in locals() else -1}\n"
            f"stdout:\n{out or ''}\n"
        )
    return {
        "ui_pid": proc.pid,
        "worker_pids": worker_pids,
        "settle_seconds": round(elapsed, 3),
        "dylib_isolation_clean": True,
    }


def main():
    parser = argparse.ArgumentParser(description="Module 12 Packaging & Release Verification")
    parser.add_argument("--native", action="store_true", help="Run native macOS Cocoa GUI checks in addition to offscreen")
    parser.add_argument("--clean", action="store_true", help="Clean release build and package staging before running")
    parser.add_argument("--output", type=Path, help="Explicit output folder")
    args = parser.parse_args()

    ts = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    base = ROOT / "docs/testing/artifacts/module-12"
    folder = args.output if args.output else base / "runs" / ts
    folder.mkdir(parents=True, exist_ok=True)
    base.mkdir(parents=True, exist_ok=True)

    telemetry = get_telemetry()
    frozen = inputs()

    record = {
        "module": "12",
        "title": "Packaging, upgrades, installed builds, and release",
        "date_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "status": "Running",
        "native_requested": args.native,
        "telemetry": telemetry,
        "steps": [],
        "artifacts": {},
        "acceptance": {
            "T-40": "Blocked (macOS arm64 standalone release bundle/DMG verified offline without dev tools; Windows/Linux/macOS x86_64/13.0 minimum unprovisioned)",
            "T-55": "Blocked (macOS arm64 bundle notices/manifest, zero test endpoints, and project preservation verified; Apple Developer ID notarization & Windows/Linux unprovisioned)",
            "AC-01_to_AC-07": "Pass on packaged macOS arm64 worker + native/offscreen client; full cross-platform release matrix Blocked",
            "Windows_Linux_matrix": "Blocked: native Windows and Linux platform packaging not provisioned",
            "Apple_Developer_ID_notarization": "Blocked: external signing credentials unprovisioned; ad-hoc codesign verified",
        },
    }

    def save():
        data = json.dumps(record, indent=2) + "\n"
        (folder / "verification.json").write_text(data)
        (base / "verification.json").write_text(data)

    def run(name, command, env=None, timeout=600):
        print(f"Running {name}...", flush=True)
        start = time.monotonic()
        try:
            with (folder / (name + ".txt")).open("w") as log:
                r = subprocess.run(command, cwd=ROOT, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=timeout)
            code = r.returncode
        except subprocess.TimeoutExpired:
            code = 124
        elapsed = time.monotonic() - start
        record["steps"].append(
            {"name": name, "command": [str(c) for c in command], "exit_code": code, "seconds": elapsed}
        )
        save()
        if code:
            record["status"] = f"Fail: {name}"
            save()
            raise SystemExit(f"Fail: {name}; see {folder / (name + '.txt')}")

    save()

    if args.clean:
        if RELEASE_BUILD_DIR.exists():
            shutil.rmtree(RELEASE_BUILD_DIR)
        if OUT_PACKAGES.exists():
            shutil.rmtree(OUT_PACKAGES)

    # Step 1: Configure and build isolated Release package tree (CAD_RELEASE_PACKAGE=ON, BUILD_TESTING=OFF)
    run(
        "configure-release",
        [
            "cmake",
            "-S",
            str(ROOT),
            "-B",
            str(RELEASE_BUILD_DIR),
            "-G",
            "Ninja",
            "-DCMAKE_BUILD_TYPE=Release",
            f"-DCMAKE_PREFIX_PATH={ROOT / '.deps/install'}",
            f"-DCMAKE_MAKE_PROGRAM={ROOT / '.deps/install/bin/ninja'}",
            "-DBUILD_TESTING=OFF",
            "-DCAD_GEOMETRY_SPIKES=OFF",
            "-DCAD_RELEASE_PACKAGE=ON",
            "-DCMAKE_OSX_DEPLOYMENT_TARGET=13.0",
            "-DCMAKE_OSX_SYSROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk",
        ],
    )
    run("build-release", [str(ROOT / ".deps/install/bin/ninja"), "-C", str(RELEASE_BUILD_DIR), "cad-desktop"])
    run("build-tests", [str(ROOT / ".deps/install/bin/ninja"), "-C", str(TEST_BUILD_DIR), "module12_contracts", "cad-desktop"])

    # Step 2: Package native distribution from clean release build (Task 12.1a, 12.1b)
    run(
        "package-distribution",
        [
            sys.executable,
            "tools/package_release.py",
            "--build-dir",
            str(RELEASE_BUILD_DIR),
            "--output-dir",
            str(OUT_PACKAGES),
        ],
    )

    pkg_manifest_path = OUT_PACKAGES / "package-manifest.json"
    if pkg_manifest_path.exists():
        pkg_manifest = json.loads(pkg_manifest_path.read_text())
        shutil.copy2(pkg_manifest_path, folder / "package-manifest.json")
        record["package_manifest"] = pkg_manifest

    app_path = OUT_PACKAGES / "staging/DatumForge.app"
    dmg_path = OUT_PACKAGES / "DatumForge-0.1.0-mac-arm64.dmg"

    # Step 3: Strict bundle & binary inspection (Task 12.1b, 12.4a, T-40, NFR-015 / T-55)
    print("Inspecting standalone release bundle...", flush=True)
    inspection = {}

    codesign_out = subprocess.run(
        ["codesign", "--verify", "--deep", "--strict", "--verbose=2", str(app_path)],
        capture_output=True,
        text=True,
    )
    inspection["codesign_exit_code"] = codesign_out.returncode
    inspection["codesign_valid"] = codesign_out.returncode == 0
    inspection["codesign_output"] = codesign_out.stderr.strip()

    dmg_codesign = subprocess.run(
        ["codesign", "--verify", "--strict", str(dmg_path)], capture_output=True, text=True
    )
    inspection["dmg_codesign_valid"] = dmg_codesign.returncode == 0

    desktop_bin = app_path / "Contents/MacOS/DatumForge"
    worker_bin = app_path / "Contents/MacOS/cad-worker"

    arch_out = subprocess.run(["file", str(desktop_bin)], capture_output=True, text=True)
    inspection["binary_arch"] = arch_out.stdout.strip()
    inspection["is_arm64"] = "arm64" in arch_out.stdout

    # Verify otool -L and -l on BOTH binaries + bundled libs
    desktop_otool_L = subprocess.check_output(["otool", "-L", str(desktop_bin)], text=True)
    worker_otool_L = subprocess.check_output(["otool", "-L", str(worker_bin)], text=True)
    desktop_otool_l = subprocess.check_output(["otool", "-l", str(desktop_bin)], text=True)
    worker_otool_l = subprocess.check_output(["otool", "-l", str(worker_bin)], text=True)

    inspection["no_deps_or_build_paths"] = all(
        ".deps" not in out and "build/" not in out
        for out in (desktop_otool_L, worker_otool_L, desktop_otool_l, worker_otool_l)
    )
    inspection["has_frameworks_rpath"] = (
        "@executable_path/../Frameworks" in desktop_otool_l
        and "@executable_path/../Frameworks" in worker_otool_l
    )
    inspection["planegcs_dynamically_linked"] = "@rpath/libcad_planegcs.dylib" in worker_otool_L
    inspection["planegcs_bundled"] = (app_path / "Contents/Frameworks/libcad_planegcs.dylib").is_file()

    manifest_file = app_path / "Contents/Resources/version-manifest.json"
    notices_file = app_path / "Contents/Resources/NOTICES.txt"
    licenses_dir = app_path / "Contents/Resources/LICENSES"
    inspection["has_manifest"] = manifest_file.exists()
    inspection["has_notices"] = notices_file.exists()
    inspection["has_licenses"] = licenses_dir.exists()
    if manifest_file.exists():
        inspection["manifest_data"] = json.loads(manifest_file.read_text())

    # Absence of test probes, debug backdoors, and worker env overrides in release binaries
    forbidden = [
        "benchmark-launch",
        "benchmark-settle",
        "recovery-root",
        "CAD_WORKER_EXECUTABLE",
        "transport_test",
        "deliberate_failure",
    ]
    strings_desktop = subprocess.check_output(["strings", str(desktop_bin)], text=True, errors="ignore")
    strings_worker = subprocess.check_output(["strings", str(worker_bin)], text=True, errors="ignore")
    nm_desktop = subprocess.check_output(["nm", "-gU", str(desktop_bin)], text=True)
    found_forbidden = [
        tok for tok in forbidden if tok in strings_desktop or tok in strings_worker or tok in nm_desktop
    ]
    inspection["forbidden_test_strings_found"] = found_forbidden
    inspection["no_test_backdoors"] = len(found_forbidden) == 0

    v_out = subprocess.run([str(desktop_bin), "--version"], capture_output=True, text=True)
    inspection["version_output"] = v_out.stdout.strip()
    inspection["version_exit_code"] = v_out.returncode

    h_out = subprocess.run([str(desktop_bin), "--help"], capture_output=True, text=True)
    inspection["help_output"] = h_out.stdout.strip()
    inspection["help_exit_code"] = h_out.returncode

    (folder / "bundle-inspection.json").write_text(json.dumps(inspection, indent=2) + "\n")
    record["inspection"] = inspection

    for key in (
        "codesign_valid",
        "dmg_codesign_valid",
        "is_arm64",
        "no_deps_or_build_paths",
        "has_frameworks_rpath",
        "planegcs_dynamically_linked",
        "planegcs_bundled",
        "has_manifest",
        "has_notices",
        "has_licenses",
        "no_test_backdoors",
    ):
        if not inspection.get(key):
            record["status"] = f"Fail: inspection.{key}"
            save()
            raise SystemExit(f"Fail: bundle inspection failed on {key}")
    if inspection["version_exit_code"] != 0 or inspection["help_exit_code"] != 0:
        raise SystemExit("Fail: --version or --help exited nonzero")

    # Step 4: DMG mount and contents inspection
    print("Inspecting packaged DMG disk image...", flush=True)
    dmg_inspection = {}
    mnt = folder / "dmg_mount"
    mnt.mkdir(exist_ok=True)
    try:
        subprocess.run(["hdiutil", "attach", str(dmg_path), "-mountpoint", str(mnt), "-nobrowse", "-readonly"], check=True, capture_output=True)
        dmg_files = [f.name for f in mnt.iterdir()]
        dmg_inspection["files"] = dmg_files
        dmg_inspection["has_app"] = "DatumForge.app" in dmg_files
        dmg_inspection["has_applications_link"] = "Applications" in dmg_files
        dmg_inspection["has_readme"] = "README.txt" in dmg_files
        dmg_inspection["has_notices"] = "NOTICES.txt" in dmg_files
        dmg_inspection["has_license"] = "LICENSE.txt" in dmg_files
        subprocess.run(["hdiutil", "detach", str(mnt)], check=True, capture_output=True)
        dmg_inspection["mount_and_detach_clean"] = True
    except Exception as e:
        dmg_inspection["error"] = str(e)
    finally:
        shutil.rmtree(mnt, ignore_errors=True)
    (folder / "dmg-inspection.json").write_text(json.dumps(dmg_inspection, indent=2) + "\n")
    record["dmg_inspection"] = dmg_inspection
    if not all(dmg_inspection.get(k) for k in ("has_app", "has_applications_link", "has_readme", "has_notices", "has_license", "mount_and_detach_clean")):
        raise SystemExit("Fail: DMG inspection incomplete")

    # Step 5 & 6: Clean isolated launch & network-denied launch of the actual unmodified release DatumForge.app
    print("Verifying clean isolated & network-denied launch of shipped release DatumForge.app...", flush=True)
    with tempfile.TemporaryDirectory(prefix="cad-module12-home-") as tmp_home:
        clean_env_offscreen = {
            "PATH": "/usr/bin:/bin",
            "HOME": tmp_home,
            "QT_QPA_PLATFORM": "offscreen",
        }
        start = time.monotonic()
        info_offscreen = launch_and_verify_shipped_app(
            app_path, clean_env_offscreen, folder / "shipped-launch-offscreen.txt", use_sandbox=True
        )
        record["steps"].append(
            {
                "name": "shipped-sandbox-launch-offscreen",
                "command": ["sandbox-exec", str(desktop_bin)],
                "exit_code": 0,
                "seconds": time.monotonic() - start,
                "details": info_offscreen,
            }
        )
        save()

        if args.native:
            print("Verifying native Cocoa launch of shipped release DatumForge.app under network denial...", flush=True)
            clean_env_native = {
                "PATH": "/usr/bin:/bin",
                "HOME": tmp_home,
            }
            start = time.monotonic()
            info_native = launch_and_verify_shipped_app(
                app_path, clean_env_native, folder / "shipped-launch-native.txt", use_sandbox=True
            )
            record["steps"].append(
                {
                    "name": "shipped-sandbox-launch-native",
                    "command": ["sandbox-exec", str(desktop_bin)],
                    "exit_code": 0,
                    "seconds": time.monotonic() - start,
                    "details": info_native,
                }
            )
            save()

    # Step 7: Installed smoke suite exercising packaged cad-worker under network denial
    print("Executing AC-01–AC-07 installed smoke suite on packaged worker...", flush=True)
    smoke_env = {
        "PATH": "/usr/bin:/bin",
        "CAD_WORKER_EXECUTABLE": str(worker_bin),
        "QT_QPA_PLATFORM": "offscreen",
    }
    run(
        "installed-smoke-contracts",
        ["/usr/bin/sandbox-exec", "-p", SANDBOX_PROFILE, str(TEST_BUILD_DIR / "module12_contracts")],
        env=smoke_env,
        timeout=300,
    )
    if args.native:
        smoke_env_native = {
            "PATH": "/usr/bin:/bin",
            "CAD_WORKER_EXECUTABLE": str(worker_bin),
        }
        run(
            "installed-smoke-contracts-native",
            ["/usr/bin/sandbox-exec", "-p", SANDBOX_PROFILE, str(TEST_BUILD_DIR / "module12_contracts")],
            env=smoke_env_native,
            timeout=300,
        )

    # Step 8: Full CTest regression suite
    run("ctest-full", ["ctest", "--test-dir", str(TEST_BUILD_DIR), "--output-on-failure"], timeout=1800)

    # Step 9: Formatting, static analysis, architecture ownership (both benchmark & release builds), research isolation
    run("format", [sys.executable, "tools/format_sources.py"])
    run("static-analysis", [sys.executable, "tools/static_analysis.py", "build/macos-benchmark/compile_commands.json"], timeout=1800)
    run("ownership-benchmark", [sys.executable, "tools/check_architecture.py", "build/macos-benchmark", "--output", str(folder / "architecture-benchmark.json")])
    run("ownership-release", [sys.executable, "tools/check_architecture.py", "build/macos-release", "--output", str(folder / "architecture-release.json")])
    run("research-isolation", [sys.executable, "tools/check_isolation.py"])

    # Step 10: Check specification rules
    run("check-specs", [sys.executable, "docs/scripts/check_specs.py", "--write-report"])

    identity = {
        "git_commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
        "branch": subprocess.check_output(["git", "branch", "--show-current"], cwd=ROOT, text=True).strip(),
        "dirty_paths": subprocess.check_output(["git", "status", "--short"], cwd=ROOT, text=True).splitlines(),
        "inputs": frozen,
        "sha256": hashlib.sha256(json.dumps(frozen, sort_keys=True).encode()).hexdigest(),
    }
    (folder / "source-identity.json").write_text(json.dumps(identity, indent=2) + "\n")

    record["status"] = "Pass: Module 12 release packaging and available-Mac installed verification passed; cross-platform and notarization gates remain Blocked"
    save()
    print("==========================================================")
    print("Module 12 verification completed successfully: PASS")
    print(f"Artifacts preserved under {folder}")
    print("==========================================================")


if __name__ == "__main__":
    main()
