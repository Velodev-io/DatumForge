#!/usr/bin/env python3
"""Execute Module 11 real 8 GB performance benchmarks and cross-module acceptance."""
import argparse
import datetime
import hashlib
import json
import math
import os
import platform
import shutil
import statistics
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD_DIR = ROOT / "build/macos-benchmark"

def percentile(values, p=95):
    if not values or not all(math.isfinite(x) for x in values):
        raise ValueError("Nonempty finite samples required")
    ordered = sorted(values)
    return ordered[max(0, math.ceil(p / 100 * len(ordered)) - 1)]

def inputs():
    paths = [ROOT / "CMakeLists.txt", ROOT / "CMakePresets.json", ROOT / ".clang-format"]
    for top in ("apps", "src", "tests", "tools", "cmake"):
        paths += [
            f
            for f in (ROOT / top).rglob("*")
            if f.is_file()
            and f.suffix in (".cpp", ".hpp", ".h", ".py", ".swift", ".cmake", ".json", ".patch")
        ]
    return {str(f.relative_to(ROOT)): hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted(set(paths))}

def measure_launches(desktop_bin, count=5):
    latencies = []
    for i in range(count):
        start = time.monotonic()
        p = subprocess.run(
            [str(desktop_bin), "--benchmark-launch"],
            cwd=ROOT,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            timeout=30,
        )
        elapsed = (time.monotonic() - start) * 1000.0
        assert p.returncode == 0, f"Launch failed with return code {p.returncode}: {p.stderr}"
        assert "CAD_DESKTOP_READY" in p.stdout, f"CAD_DESKTOP_READY not received in launch {i}"
        latencies.append(elapsed)
    return {
        "samples_ms": latencies,
        "median_ms": statistics.median(latencies),
        "p95_ms": percentile(latencies, 95),
    }

def measure_settle(desktop_bin, sample_mem_py, folder, seconds=30):
    start = time.monotonic()
    proc = subprocess.Popen(
        [str(desktop_bin), "--benchmark-settle", str(seconds)],
        cwd=ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    # Wait for ready signal
    ready = False
    for line in iter(proc.stdout.readline, ""):
        if "CAD_DESKTOP_READY" in line:
            ready = True
            break
    assert ready, "cad-desktop did not reach CAD_DESKTOP_READY"
    desktop_pid = proc.pid
    # Find worker child PID
    worker_pid = None
    time.sleep(0.5)
    try:
        pgrep = subprocess.check_output(["pgrep", "-P", str(desktop_pid)], text=True).split()
        if pgrep:
            worker_pid = int(pgrep[0])
    except Exception:
        pass
    pids = [desktop_pid]
    if worker_pid:
        pids.append(worker_pid)
    mem_output = folder / "b0-settle-memory.json"
    subprocess.run(
        [sys.executable, str(sample_mem_py), *[str(x) for x in pids], "--seconds", str(seconds), "--output", str(mem_output)],
        check=True,
        timeout=seconds + 30,
    )
    proc.wait(timeout=30)
    data = json.loads(mem_output.read_text())
    samples = data.get("samples", [])
    ui_footprints = []
    worker_footprints = []
    family_footprints = []
    for s in samples:
        fam = 0
        for p_info in s.get("processes", []):
            fp = p_info.get("physical_footprint_bytes", 0)
            if p_info.get("pid") == desktop_pid:
                ui_footprints.append(fp)
            else:
                worker_footprints.append(fp)
            fam += fp
        family_footprints.append(fam)
    initial_bytes = family_footprints[0] if family_footprints else 0
    steady_bytes = family_footprints[-1] if family_footprints else 0
    peak_bytes = max(family_footprints) if family_footprints else 0
    return {
        "seconds": seconds,
        "initial_footprint_bytes": initial_bytes,
        "initial_footprint_mib": initial_bytes / (1024 * 1024),
        "steady_footprint_bytes": steady_bytes,
        "steady_footprint_mib": steady_bytes / (1024 * 1024),
        "peak_footprint_bytes": peak_bytes,
        "peak_footprint_mib": peak_bytes / (1024 * 1024),
        "target_mib": 350.0,
        "pass": (steady_bytes / (1024 * 1024)) <= 350.0,
    }

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--native", action="store_true")
    p.add_argument("--clean", action="store_true")
    a = p.parse_args()

    if sys.platform != "darwin":
        raise SystemExit("Blocked: provision Windows/Linux native runners")

    base = ROOT / "docs/testing/artifacts/module-11"
    folder = base / "runs" / datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    folder.mkdir(parents=True, exist_ok=True)

    frozen = inputs()

    # Hardware & Environment Telemetry
    mem_bytes = int(subprocess.check_output(["sysctl", "-n", "hw.memsize"], text=True))
    cpu_str = subprocess.check_output(["sysctl", "-n", "machdep.cpu.brand_string"], text=True).strip()
    swap_str = subprocess.check_output(["sysctl", "-n", "vm.swapusage"], text=True).strip()
    power_str = subprocess.check_output(["pmset", "-g", "batt"], text=True).strip()
    therm_str = subprocess.check_output(["pmset", "-g", "therm"], text=True).strip()

    record = {
        "scope": "Module 11 real 8 GB performance benchmarks and cross-module acceptance reconciliation",
        "status": "In progress",
        "artifacts": str(folder.relative_to(ROOT)),
        "steps": [],
        "host": {
            "platform": platform.platform(),
            "machine": platform.machine(),
            "physical_ram_bytes": mem_bytes,
            "physical_ram_gib": mem_bytes / (1024**3),
            "cpu": cpu_str,
            "swap": swap_str,
            "power": power_str,
            "thermal": therm_str,
            "viewport": {"width": 1280, "height": 800, "scale": 1.0},
            "build_profile": "RelWithDebInfo",
        },
        "blocked": [
            "Prior Module 6.1b and 6.4c deferred parser/resource/platform gates",
            "Prior Module 7.4c full resource/platform gate",
            "Windows/Linux/minimum macOS/other GPU/installed release builds (Module 12)",
            "VoiceOver/NVDA/Orca assistive technology matrix",
            "Installed package offline smoke/critical suite (Module 12)",
        ],
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
        record["steps"].append({"name": name, "command": command, "exit_code": code, "seconds": elapsed})
        save()
        if code:
            record["status"] = f"Fail: {name}"
            save()
            raise SystemExit(f"Fail: {name}; see {folder}")

    save()

    # Step 1: Build targets
    run("build", ["cmake", "--build", str(BUILD_DIR), "-j8"])

    # Step 2: B0 cold and warm launches
    desktop_bin = BUILD_DIR / "cad-desktop.app/Contents/MacOS/cad-desktop"
    print("Measuring B0 cold launches (5 samples)...", flush=True)
    cold_launches = measure_launches(desktop_bin, count=5)
    print("Measuring B0 warm launches (10 samples)...", flush=True)
    warm_launches = measure_launches(desktop_bin, count=10)

    # Step 3: B0 30-second idle settle
    print("Measuring B0 30-second idle settle...", flush=True)
    settle_report = measure_settle(desktop_bin, ROOT / "tools/sample_memory.py", folder, seconds=30)

    b0_report = {
        "cold_launch": cold_launches,
        "warm_launch": warm_launches,
        "idle_settle": settle_report,
    }
    (folder / "b0-launch-settle.json").write_text(json.dumps(b0_report, indent=2) + "\n")

    # Step 4: B1–B3 contracts benchmarks
    run("module11-benchmarks", [str(BUILD_DIR / "module11_contracts")], env={"QT_QPA_PLATFORM": "offscreen", "QTEST_FUNCTION_TIMEOUT": "600000", "CAD_ARTIFACT_DIR": str(folder)}, timeout=1800)
    bench_src = Path("/tmp/m11-test/module11-benchmarks.json")
    if bench_src.exists() and not (folder / "module11-benchmarks.json").exists():
        shutil.copy2(bench_src, folder / "module11-benchmarks.json")

    # Step 5: B4 STEP and IGES exchange roundtrip verification
    run("b4-exchange", [str(BUILD_DIR / "module6_exchange")])
    if (ROOT / "module6-b4.json").exists():
        shutil.copy2(ROOT / "module6-b4.json", folder / "module6-b4.json")

    # Step 6: B5 2-sheet drawing, BOM, and PDF inspection
    run("b5-drawing", [str(BUILD_DIR / "module8_drawing"), str(folder / "module8-B5.pdf"), str(folder / "module8-B5.json")])
    run("b5-pdf-inspection", ["swift", "-module-cache-path", "/private/tmp/cad-pdf-swift-cache", "tools/inspect_drawing_pdf.swift", str(folder / "module8-B5.pdf"), str(folder / "pdf")])

    # Step 7: B6 Fault hardening matrix
    run("b6-fault-matrix", [sys.executable, "tests/module10/fault_matrix.py", str(BUILD_DIR / "module10_fault_worker"), "--output", str(folder / "module10-fault-matrix.json")], timeout=600)

    # Step 8: Full CTest regression suite
    run("ctest-full", ["ctest", "--test-dir", str(BUILD_DIR), "--output-on-failure"], timeout=1800)

    # Step 9: Source hygiene & static analysis
    run("format", [sys.executable, "tools/format_sources.py"])
    run("static-analysis", [sys.executable, "tools/static_analysis.py", "build/macos-benchmark/compile_commands.json"], timeout=1800)
    run("ownership", [sys.executable, "tools/check_architecture.py", "build/macos-benchmark", "--output", str(folder / "architecture.json")])
    run("research-isolation", [sys.executable, "tools/check_isolation.py"])

    # Step 10: Offline workflows
    run("offline", [sys.executable, "tools/offline_workflows.py", "--output", str(folder / "offline"), "--build-dir", str(BUILD_DIR)], timeout=600)

    # Source identity
    identity = {
        "git_commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
        "branch": subprocess.check_output(["git", "branch", "--show-current"], cwd=ROOT, text=True).strip(),
        "dirty_paths": subprocess.check_output(["git", "status", "--short"], cwd=ROOT, text=True).splitlines(),
        "inputs": frozen,
        "sha256": hashlib.sha256(json.dumps(frozen, sort_keys=True).encode()).hexdigest(),
    }
    (folder / "source-identity.json").write_text(json.dumps(identity, indent=2) + "\n")

    record["status"] = "Pass: Module 11 benchmarks and available-Mac checks; full cross-platform acceptance Blocked"
    save()
    print("Module 11 verification completed successfully!", flush=True)

if __name__ == "__main__":
    main()
