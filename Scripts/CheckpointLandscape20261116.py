"""Checkpoint the exact Landscape candidate and hash its protected inputs.

Run explicitly with the UE-bundled Python interpreter. This script never starts
Unreal, edits content, removes files, or imports project helper modules. A failed
run leaves its new backup files in place for inspection and cannot be retried
over that destination. The successful manifest is written last, exclusively.
"""

from __future__ import annotations

import sys

sys.dont_write_bytecode = True

from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import stat


PROJECT = Path("D:/Project/Rogue10m")
BACKUP = PROJECT / "tmp/LandscapeProduction20261116A/before"
MANIFEST = PROJECT / "Feature/doc/evidence/landscape-production-20261116/baseline-files.json"
CANDIDATE = "Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A"
PROTECTED_MAPS = (
    "Rogue10m/Maps/StartHub",
    "Rogue10m/Maps/Production/WorldLayout20261112",
    "Rogue10m/Maps/Production/WorldLayoutCompact20261113A",
)
REPARSE_POINT = getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400)
CHUNK_BYTES = 1024 * 1024


def require(condition: bool, message: str) -> None:
    # Explicit checks remain active if Python is run with -O.
    if not condition:
        raise RuntimeError(message)


def lexical_path(path: Path) -> Path:
    return Path(os.path.abspath(os.fspath(path)))


def no_links(path: Path) -> Path:
    """Check every existing parent before allowing Path.resolve or file I/O."""
    absolute = lexical_path(path)
    for current in reversed((absolute, *absolute.parents)):
        try:
            info = current.lstat()
        except FileNotFoundError:
            continue
        require(
            not stat.S_ISLNK(info.st_mode)
            and not (getattr(info, "st_file_attributes", 0) & REPARSE_POINT),
            "Symlink/reparse path is forbidden: " + str(current),
        )
    return absolute


def project_path(path: Path) -> Path:
    absolute = no_links(path)
    require(
        os.path.commonpath((str(PROJECT), str(absolute))).casefold()
        == str(PROJECT).casefold(),
        "Path escaped the exact project: " + str(absolute),
    )
    return absolute


def signature(info: os.stat_result) -> tuple[int, int, int, int]:
    return info.st_dev, info.st_ino, info.st_size, info.st_mtime_ns


def pin(path: Path) -> dict:
    source = project_path(path)
    before = source.lstat()
    require(stat.S_ISREG(before.st_mode), "Expected regular file: " + str(source))
    digest = hashlib.sha256()
    count = 0
    with source.open("rb") as stream:
        require(signature(os.fstat(stream.fileno())) == signature(before),
                "File changed before reading: " + str(source))
        while chunk := stream.read(CHUNK_BYTES):
            digest.update(chunk)
            count += len(chunk)
        require(signature(os.fstat(stream.fileno())) == signature(before),
                "File changed while reading: " + str(source))
    after = project_path(source).lstat()
    require(signature(after) == signature(before) and count == before.st_size,
            "File changed during hashing: " + str(source))
    return {
        "path": source.relative_to(PROJECT).as_posix(),
        "size": count,
        "sha256": digest.hexdigest().upper(),
        "mtime_ns": before.st_mtime_ns,
    }


def tree_files(directory: Path, required: bool) -> list[Path]:
    root = project_path(directory)
    if not root.exists():
        require(not required, "Required directory is missing: " + str(root))
        return []
    require(root.is_dir(), "Expected directory: " + str(root))
    result = []
    pending = [root]
    while pending:
        folder = project_path(pending.pop())
        # Do not traverse entries until their reparse/symlink flags are checked.
        with os.scandir(folder) as entries:
            for entry in entries:
                path = project_path(Path(entry.path))
                info = entry.stat(follow_symlinks=False)
                if stat.S_ISDIR(info.st_mode):
                    pending.append(path)
                else:
                    require(stat.S_ISREG(info.st_mode), "Special file is forbidden: " + str(path))
                    result.append(path)
    return sorted(result, key=lambda value: value.as_posix().casefold())


def map_files(package: str) -> tuple[list[Path], list[dict]]:
    asset = project_path(PROJECT / "Content" / (package + ".umap"))
    require(asset.is_file(), "Required map is missing: " + str(asset))
    files = [asset]
    directories = []
    for container in ("__ExternalActors__", "__ExternalObjects__"):
        directory = project_path(PROJECT / "Content" / container / package)
        entries = tree_files(directory, required=container == "__ExternalActors__")
        require(container != "__ExternalActors__" or bool(entries),
                "Required WP actor directory is empty: " + str(directory))
        files.extend(entries)
        directories.append({
            "path": directory.relative_to(PROJECT).as_posix(),
            "exists": directory.exists(),
            "files": len(entries),
        })
    return files, directories


def make_directory(path: Path) -> None:
    destination = project_path(path)
    destination.mkdir(parents=True, exist_ok=True)
    project_path(destination)
    require(destination.is_dir(), "Expected destination directory: " + str(destination))


def same_content(actual: dict, expected: dict) -> bool:
    return actual["path"] == expected["path"] and actual["size"] == expected["size"] \
        and actual["sha256"] == expected["sha256"]


def main() -> int:
    require(os.name == "nt", "This checkpoint is restricted to Windows.")
    project = no_links(PROJECT)
    require(project.is_dir(), "Exact project directory is missing.")
    require(no_links(Path(__file__)).parent.parent.samefile(project),
            "Script must run from the exact D:/Project/Rogue10m checkout.")
    require(project_path(PROJECT / "Rogue10m.uproject").is_file(),
            "Rogue10m.uproject is missing.")
    require(not project_path(BACKUP).exists(), "Fresh backup destination already exists: " + str(BACKUP))
    require(not project_path(MANIFEST).exists(), "Manifest already exists: " + str(MANIFEST))

    candidate_files, candidate_directories = map_files(CANDIDATE)
    protected_files = []
    protected_directories = []
    for relative in ("Source", "Config", "Content/Rogue10m/World/Production"):
        folder = project_path(PROJECT / relative)
        entries = tree_files(folder, required=True)
        protected_files.extend(entries)
        protected_directories.append({"path": relative, "exists": True, "files": len(entries)})
    for package in PROTECTED_MAPS:
        entries, directories = map_files(package)
        protected_files.extend(entries)
        protected_directories.extend(directories)
    candidate_set = set(candidate_files)
    protected_set = set(protected_files)
    require(not candidate_set.intersection(protected_set), "Backup and protected scopes overlap.")
    candidate_pins = [pin(path) for path in candidate_files]
    protected_pins = [pin(path) for path in sorted(protected_set, key=str)]

    # Create the leaf exclusively. No deletion, cleanup, replacement or symlink.
    make_directory(BACKUP.parent)
    project_path(BACKUP).mkdir(exist_ok=False)
    copied_bytes = 0
    for source, expected in zip(candidate_files, candidate_pins):
        require(same_content(pin(source), expected), "Candidate changed before copy: " + str(source))
        destination = project_path(BACKUP / expected["path"])
        require(destination.is_relative_to(BACKUP), "Backup path escaped its leaf.")
        make_directory(destination.parent)
        digest = hashlib.sha256()
        count = 0
        with source.open("rb") as incoming, destination.open("xb") as outgoing:
            while chunk := incoming.read(CHUNK_BYTES):
                outgoing.write(chunk)
                digest.update(chunk)
                count += len(chunk)
            outgoing.flush()
            os.fsync(outgoing.fileno())
        require(count == expected["size"] and digest.hexdigest().upper() == expected["sha256"],
                "Copy stream failed verification: " + str(source))
        actual_backup = pin(destination)
        require(actual_backup["size"] == expected["size"] and actual_backup["sha256"] == expected["sha256"],
                "Backup readback failed verification: " + str(destination))
        copied_bytes += count

    # Re-enumerate all scoped inputs to detect added/removed files during copy.
    current_candidate, _ = map_files(CANDIDATE)
    current_protected = []
    for relative in ("Source", "Config", "Content/Rogue10m/World/Production"):
        current_protected.extend(tree_files(PROJECT / relative, required=True))
    for package in PROTECTED_MAPS:
        entries, _ = map_files(package)
        current_protected.extend(entries)
    require(set(current_candidate) == candidate_set and set(current_protected) == protected_set,
            "Scoped file inventory changed during checkpoint.")
    for expected in (*candidate_pins, *protected_pins):
        require(same_content(pin(PROJECT / expected["path"]), expected),
                "Scoped input changed during checkpoint: " + expected["path"])

    report = {
        "status": "CANDIDATE_PHYSICAL_BACKUP_SHA256_VERIFIED_PROTECTED_INPUTS_UNCHANGED",
        "recorded_utc": datetime.now(timezone.utc).isoformat(),
        "project": str(PROJECT),
        "candidate_map": "/Game/" + CANDIDATE,
        "backup_root": BACKUP.relative_to(PROJECT).as_posix(),
        "backup_scope": "Only candidate .umap and its exact WP external actor/object packages.",
        "protected_hash_scope": "Source, Config, StartHub, WorldLayout20261112, WorldLayoutCompact20261113A and their WP packages; Content/Rogue10m/World/Production.",
        "candidate_directories": candidate_directories,
        "protected_directories": protected_directories,
        "backed_up_files": candidate_pins,
        "protected_files": protected_pins,
        "backed_up_file_count": len(candidate_pins),
        "backed_up_bytes": copied_bytes,
        "protected_file_count": len(protected_pins),
        "protected_bytes": sum(row["size"] for row in protected_pins),
        "source_inputs_changed": [],
        "vendors_or_previous_evidence_edited": 0,
        "unreal_processes_launched": 0,
        "asset_mutations": 0,
        "deletions": 0,
        "no_reparse_paths_verified": True,
        "limitations": "Filesystem checkpoint only; no unsaved-editor-state or runtime correctness assertion.",
    }
    make_directory(MANIFEST.parent)
    with project_path(MANIFEST).open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(report, stream, ensure_ascii=False, indent=2)
        stream.write("\n")
        stream.flush()
        os.fsync(stream.fileno())
    print(json.dumps({key: report[key] for key in (
        "status", "backup_root", "backed_up_file_count", "backed_up_bytes",
        "protected_file_count", "protected_bytes", "asset_mutations",
    )}, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
