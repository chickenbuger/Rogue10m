"""Run the reviewed 43 native tests in a hidden Editor CLI; no GUI input or captures.

The shared owner must first verify no other UE/build/game process is active.
"""
from __future__ import annotations

import argparse
import datetime
import hashlib
import json
from pathlib import Path
import subprocess
import time


ROOT = Path(__file__).resolve().parents[1]
EVIDENCE = ROOT / "Feature/doc/evidence/map-resume-20261113"
ENGINE = Path("D:/Program Files/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe")
SELECTION = ROOT / "Feature/implementation/map-resume-20261113/native-no-gui-tests-20261113.json"


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--execute", action="store_true")
    parser.add_argument("--label", choices=("NativeTestsA", "NativeTestsB", "NativeTestsC", "NativeTestsD"), default="NativeTestsA")
    args = parser.parse_args()
    assert sha(SELECTION) == "389c520aff9c1f3b3358270c67480d427116dac1b4d368ad429aea45dded99c8"
    selection = json.loads(SELECTION.read_text(encoding="utf-8-sig"))
    rows = selection["tests"]
    names = [r if isinstance(r, str) else r["name"] for r in rows]
    assert len(names) == 43 and len(set(names)) == 43
    assert all(n.startswith(("Rogue10m.AuthoredContactSequence.", "Rogue10m.Authoring.G08.",
                             "Rogue10m.Combat.VfxNearSideBoundsPlacement", "Rogue10m.VillageQuest."))
               and not any(c in n for c in ('+', '^', '$', '"', '\n', '\r')) for n in names)
    exact_filter = "+".join("^" + n + "$" for n in names)
    out = EVIDENCE / args.label
    command = [str(ENGINE), str(ROOT / "Rogue10m.uproject"), "/Engine/Maps/Entry", "-unattended", "-NullRHI",
               "-NoSound", "-NoSplash", "-NoLiveCoding", "-nop4", "-stdout",
               "-FullStdOutLogOutput", "-ExecCmds=Automation RunTests " + exact_filter,
               "-TestExit=Automation Test Queue Empty", "-ReportExportPath=" + str(out / "report"),
               "-abslog=" + str(out / "editor.log"), "-culture=en", "-language=en", "-locale=en-US"]
    if not args.execute:
        print(json.dumps({"status": "plan_only", "command": command, "tests": names}, indent=2))
        return
    out.mkdir(exist_ok=False)
    inputs = list((ROOT / "Source").rglob("*")) + list((ROOT / "Config").rglob("*"))
    files = {str(p.relative_to(ROOT)): sha(p) for p in inputs if p.is_file()}
    dll = ROOT / "Binaries/Win64/UnrealEditor-Rogue10m.dll"
    before_dll = sha(dll)
    record = {"status": "running", "started_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
              "dll_sha256": before_dll, "selection_sha256": sha(SELECTION), "tests": names,
              "source_config_sha256": files, "command": command, "gui_input_operations": 0,
              "boundary": "Native numeric/reflection/world/physics/timer tests only; no actual animation, input or UI certification"}
    started = time.monotonic()
    with (out / "stdout.log").open("w", encoding="utf-8") as stdout, (out / "stderr.log").open("w", encoding="utf-8") as stderr:
        process = subprocess.Popen(command, cwd=ROOT, stdout=stdout, stderr=stderr,
                                   creationflags=subprocess.CREATE_NO_WINDOW)
        record["pid"] = process.pid
        (out / "process.json").write_text(json.dumps(record, indent=2), encoding="utf-8")
        try:
            code = process.wait(timeout=600)
        except subprocess.TimeoutExpired:
            process.kill()  # Only the exact owned commandlet process handle.
            process.wait(timeout=30)
            record.update(status="owned_commandlet_timeout", timeout_seconds=600)
            (out / "result.json").write_text(json.dumps(record, indent=2), encoding="utf-8")
            raise
    after_inputs = list((ROOT / "Source").rglob("*")) + list((ROOT / "Config").rglob("*"))
    after_files = {str(p.relative_to(ROOT)): sha(p) for p in after_inputs if p.is_file()}
    record.update(exit_code=code, elapsed_seconds=time.monotonic() - started,
                  ended_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                  same_dll=sha(dll) == before_dll,
                  source_config_unchanged=after_files == files)
    record["status"] = "headless_editor_exited_report_review_required"
    (out / "result.json").write_text(json.dumps(record, indent=2), encoding="utf-8")
    print(json.dumps({k: v for k, v in record.items() if k not in ("source_config_sha256", "command", "tests")}, indent=2))
    assert code == 0 and record["same_dll"] and record["source_config_unchanged"], "Test process failed or inputs changed"


if __name__ == "__main__":
    main()
