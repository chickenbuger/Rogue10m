"""Run the editor-only map maker suite in an isolated unattended Unreal process."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import time


def snapshot(project: Path) -> dict[str, str]:
    roots = [project / "Content/Rogue10m/Maps/OpenRunMap.umap"]
    roots += [project / "Content" / category / "Rogue10m/Maps/OpenRunMap"
              for category in ("__ExternalActors__", "__ExternalObjects__")]
    files = []
    for root in roots:
        files.extend(root.rglob("*.*") if root.is_dir() else [root])
    hashes = {}
    for path in sorted(files):
        if path.is_file():
            with path.open("rb") as stream:
                hashes[str(path.relative_to(project))] = hashlib.file_digest(stream, "sha256").hexdigest()
    return hashes


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine-root", type=Path, default=Path(r"D:\Program Files\UE_5.8"))
    parser.add_argument("--output", type=Path)
    parser.add_argument("--timeout", type=int, default=900)
    parser.add_argument("--cleanup-fixture", help="Clean orphan external packages of one explicitly named prior test fixture.")
    args = parser.parse_args()
    if args.cleanup_fixture and not re.fullmatch(r"/Game/__MapMakerTests/MapMakerFixture_[0-9A-Fa-f]{32}", args.cleanup_fixture):
        parser.error("cleanup fixture must be an exact generated MapMakerFixture GUID package")
    project = Path(__file__).resolve().parents[1]
    output = (args.output or project / "tmp" / f"map-maker-tests-{time.time_ns()}").resolve()
    output.mkdir(parents=True, exist_ok=False)
    report = output / "report"
    log = output / "automation.log"
    before = snapshot(project)
    if "Content/Rogue10m/Maps/OpenRunMap.umap" not in {name.replace("\\", "/") for name in before}:
        raise RuntimeError("Original OpenRunMap is missing; refusing an unverifiable integrity check.")
    (output / "original-map-before.json").write_text(json.dumps(before, indent=2), encoding="utf-8")
    command = [str(args.engine_root / "Engine/Binaries/Win64/UnrealEditor-Cmd.exe"),
               str(project / "Rogue10m.uproject"), "-Unattended", "-NullRHI", "-NoSplash", "-NoSound",
               "-NoLiveCoding", "-MapMakerIntegrationTest", "-ExecCmds=Automation RunTests Rogue10m.MapMaker",
               "-TestExit=Automation Test Queue Empty", f"-ReportExportPath={report}", f"-abslog={log}"]
    if args.cleanup_fixture:
        command.append(f"-MapMakerCleanupFixture={args.cleanup_fixture}")
    start = time.monotonic()
    timed_out = False
    with (output / "stdout.log").open("wb") as stream:
        process = subprocess.Popen(command, cwd=project, stdout=stream, stderr=subprocess.STDOUT)
        try:
            exit_code = process.wait(timeout=args.timeout)
        except subprocess.TimeoutExpired:
            timed_out = True
            subprocess.run(["taskkill", "/PID", str(process.pid), "/T", "/F"], capture_output=True, check=False)
            process.wait(timeout=30)
            exit_code = process.returncode
    after = snapshot(project)
    changed = sorted(key for key in before.keys() | after.keys() if before.get(key) != after.get(key))
    index = report / "index.json"
    data = json.loads(index.read_text(encoding="utf-8-sig")) if index.is_file() else {}
    expected = {"Rogue10m.MapMaker.GridBounds", "Rogue10m.MapMaker.EnvironmentPolicy",
                "Rogue10m.MapMaker.EditorIntegration"}
    tests = {test.get("fullTestPath"): test.get("state") for test in data.get("tests", [])}
    passed = (set(tests) == expected and all(state == "Success" for state in tests.values())
              and all(data.get(field) == 0 for field in ("failed", "notRun", "inProcess")))
    result = {"success": exit_code == 0 and not timed_out and passed and not changed,
              "exit_code": exit_code, "timed_out": timed_out, "seconds": round(time.monotonic() - start, 2),
              "tests": tests, "required_tests_passed": passed, "original_map_file_count": len(before),
              "changed_original_map_files": changed, "command": command, "output": str(output)}
    (output / "result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(json.dumps(result, indent=2))
    return 0 if result["success"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
