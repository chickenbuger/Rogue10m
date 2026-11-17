"""Read-only project snapshot plus independent backups for the map owner.

Writes only to one fresh feature-evidence directory. Never starts Unreal, builds,
changes Git state, or overwrites a source/asset/existing checkpoint.
"""
from __future__ import annotations

import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import stat
import subprocess

ROOT = Path(__file__).resolve().parents[1]
EVIDENCE = ROOT / "Feature/doc/evidence/map-resume-20261113"
BACKUP_INPUTS = [
    "Source", "Config", "Content/Rogue10m/Maps/Production/WorldLayout20261112.umap",
    "Content/__ExternalActors__/Rogue10m/Maps/Production/WorldLayout20261112",
    "Content/__ExternalObjects__/Rogue10m/Maps/Production/WorldLayout20261112",
    "Content/Rogue10m/World/Production/WorldLayout20261112",
    "Content/Rogue10m/Maps/StartHub.umap", "Content/__ExternalActors__/Rogue10m/Maps/StartHub",
    "Content/__ExternalObjects__/Rogue10m/Maps/StartHub",
]
COMPACT_INPUTS = [
    "Content/Rogue10m/Maps/Production/WorldLayoutCompact20261113A.umap",
    "Content/__ExternalActors__/Rogue10m/Maps/Production/WorldLayoutCompact20261113A",
    "Content/__ExternalObjects__/Rogue10m/Maps/Production/WorldLayoutCompact20261113A",
]


def digest(path):
    result = hashlib.sha256()
    with path.open("rb") as stream:
        for data in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(data)
    return result.hexdigest()


def reparse(path):
    return bool(getattr(path.lstat(), "st_file_attributes", 0) & stat.FILE_ATTRIBUTE_REPARSE_POINT)


def git(*args):
    return subprocess.run(["git", "-c", "safe.directory=" + str(ROOT), *args], cwd=ROOT,
                          check=True, capture_output=True, text=True, encoding="utf-8").stdout


def capture(label, compact_only=False):
    if not re.fullmatch(r"[A-Za-z0-9_-]{1,32}", label):
        raise ValueError("Label must be ASCII 1..32 characters.")
    if reparse(ROOT) or reparse(ROOT / "Content"):
        raise ValueError("Project and Content must be independent physical directories.")
    destination = EVIDENCE / label
    if destination.exists():
        raise FileExistsError("Checkpoint exists; it will not be overwritten: " + str(destination))
    for parent in (EVIDENCE, EVIDENCE.parent, EVIDENCE.parent.parent):
        if parent.exists() and reparse(parent):
            raise ValueError("Evidence path contains a reparse point.")
    branch = git("branch", "--show-current").strip()
    if not branch.startswith("Sprint#"):
        raise ValueError("Expected an existing Sprint work branch; no branch switch is performed.")
    tracked_before = git("status", "--porcelain=v1", "-uno")
    paths = set()
    missing = []
    for relative in COMPACT_INPUTS if compact_only else BACKUP_INPUTS:
        source = ROOT / relative
        if not source.exists():
            missing.append(relative)
            continue
        if reparse(source):
            raise ValueError("Backup input is a reparse point: " + relative)
        if source.is_file():
            paths.add(source)
            continue
        for directory, directories, filenames in os.walk(source, followlinks=False):
            base = Path(directory)
            if any(reparse(base / name) for name in directories + filenames):
                raise ValueError("Backup input contains a reparse point: " + str(base))
            paths.update(base / name for name in filenames)
    if compact_only and missing:
        raise FileNotFoundError("A saved compact map and its external packages are required: " + str(missing))
    destination.mkdir(parents=True, exist_ok=False)
    report = {"schema_version": 1, "label": label, "status": "copying",
              "started_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
              "project": str(ROOT), "branch": branch, "tracked_status_before": tracked_before,
              "scope": "compact_candidate_before_editor_changes" if compact_only else "original_source_config_worlds",
              "missing_optional_inputs": missing, "files": [], "original_modified": False,
              "ownership": "parent assigned this Work exclusive shared UE/build ownership; external Codex still respected",
              "backup_kind": "physical independent byte copies; no Content junctions or Git mutations"}
    try:
        for source in sorted(paths):
            relative = source.relative_to(ROOT)
            before = digest(source)
            copy = destination / "backup" / relative
            copy.parent.mkdir(parents=True, exist_ok=True)
            with source.open("rb") as reader, copy.open("xb") as writer:
                shutil.copyfileobj(reader, writer)
            if digest(source) != before or digest(copy) != before:
                raise RuntimeError("Input changed during backup; inspect external work: " + str(relative))
            report["files"].append({"path": relative.as_posix(), "sha256": before, "bytes": copy.stat().st_size})
        dll = ROOT / "Binaries/Win64/UnrealEditor-Rogue10m.dll"
        report["dll"] = {"exists": dll.exists(), "sha256": digest(dll) if dll.exists() else None}
        report["tracked_status_after"] = git("status", "--porcelain=v1", "-uno")
        report["tracked_status_unchanged"] = report["tracked_status_after"] == tracked_before
        if not report["tracked_status_unchanged"]:
            raise RuntimeError("Tracked workspace state changed during snapshot; no changes were reverted.")
        report["status"] = "snapshot_and_backups_verified"
    except Exception as error:
        report["status"] = "incomplete_preserved_for_inspection"
        report["error"] = str(error)
        raise
    finally:
        report["finished_utc"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
        with (destination / "checkpoint.json").open("x", encoding="utf-8", newline="\n") as stream:
            json.dump(report, stream, ensure_ascii=False, indent=2)
            stream.write("\n")
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--label", required=True)
    parser.add_argument("--compact-only", action="store_true")
    args = parser.parse_args()
    result = capture(args.label, args.compact_only)
    print(json.dumps({"status": result["status"], "files": len(result["files"]),
                      "branch": result["branch"], "original_modified": False}, ensure_ascii=False))
