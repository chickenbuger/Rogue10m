"""Freeze the pre-cave disk checkpoint without operating the Unreal Editor.

Only copies the exact candidate's map/EA/EO packages and eight existing owned
assets. Source/config/original maps/production inputs and non-selected saves
are read and hashed, never edited. Fresh destinations and all readbacks are
required. A failure preserves partial files and is never retried in place.
"""
from __future__ import annotations

from datetime import datetime, timezone
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import sys

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("cave_checkpoint_base", ROOT / "Scripts/CheckpointLandscape20261116.py")
BASE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(BASE)
BACKUP = ROOT / "tmp/UndergroundCaves20261118/before"
MANIFEST = ROOT / "Feature/doc/evidence/caves20261118/baseline-files.json"
OWN = "Content/Rogue10m/World/Production/WorldLayoutCompactCrop20261114A/NaturalBoundary20261117"
EXPECTED_OWN = {"DA_IntroSupply_Flat.uasset", "M_RoadFeather.uasset", "M_VillageEarth.uasset"} | {
    "M_Natural_" + name + ".uasset" for name in (
        "DarkTimber", "WarmStone", "Banner_Teal", "Banner_Ochre", "Banner_Blue")}
REFERENCE = "a7bb4af392ed9356b2a245f200206188a3ad6535"


def selected_qa_save(path: Path) -> bool:
    name = path.name
    if name in {"Rogue10m_Town_QA_Landscape20261116A.sav", "Rogue10m_Fighter_QA_Landscape20261116A.sav"}:
        return True
    return re.fullmatch(
        r"Rogue10m_Progression_QA_Landscape20261116A_AC920A0E4D79EBE2C1F3EEBE945FD67A(?:_v\d+)?\.sav", name
    ) is not None


def git(*args: str) -> str:
    return subprocess.check_output(["git", *args], cwd=ROOT).decode("utf-8").strip()


def enumerate_inputs():
    candidate, candidate_dirs = BASE.map_files(BASE.CANDIDATE)
    owned = BASE.tree_files(ROOT / OWN, required=True)
    protected, protected_dirs = [], []
    for relative in ("Source", "Config", "Content/Rogue10m/World/Production"):
        entries = BASE.tree_files(ROOT / relative, required=True)
        protected.extend(entries)
        protected_dirs.append({"path": relative, "files": len(entries)})
    for package in BASE.PROTECTED_MAPS:
        entries, directories = BASE.map_files(package)
        protected.extend(entries)
        protected_dirs.extend(directories)
    all_saves = BASE.tree_files(ROOT / "Saved/SaveGames", required=True)
    selected_saves = [p for p in all_saves if selected_qa_save(p)]
    saves = [p for p in all_saves if not selected_qa_save(p)]
    return candidate, owned, sorted(set(protected), key=str), saves, selected_saves, candidate_dirs, protected_dirs


def fingerprint(rows: list[dict]) -> str:
    canonical = [{k: row[k] for k in ("path", "size", "sha256")} for row in sorted(rows, key=lambda v: v["path"])]
    return hashlib.sha256(json.dumps(canonical, sort_keys=True, separators=(",", ":")).encode()).hexdigest()


def main() -> int:
    BASE.require(ROOT == Path("D:/Project/Rogue10m"), "Exact checkout required")
    BASE.require(BASE.no_links(ROOT).is_dir() and BASE.no_links(ROOT / "Content").is_dir(), "Physical project/content required")
    BASE.require(not BASE.project_path(BACKUP).exists(), "Fresh backup leaf already exists")
    BASE.require(not BASE.project_path(MANIFEST).exists(), "Fresh manifest already exists")
    # Git is only read. Unrelated documentation commits do not invalidate the
    # last saved-map checkpoint; all relevant source/content paths must match it.
    scope = ["Source", "Config", "Content/" + BASE.CANDIDATE + ".umap",
             "Content/__ExternalActors__/" + BASE.CANDIDATE,
             "Content/__ExternalObjects__/" + BASE.CANDIDATE, OWN]
    current_head = git("rev-parse", "HEAD")
    BASE.require(not git("diff", "--name-only", REFERENCE, "--", *scope), "Scoped tracked inputs differ from saved-map commit")
    BASE.require(not git("ls-files", "--others", "--exclude-standard", "--", *scope), "Untracked source/content before cave checkpoint")
    candidate, owned, protected, saves, selected, candidate_dirs, protected_dirs = enumerate_inputs()
    BASE.require(len(candidate) == 1473, "Expected exact1473 candidate packages")
    BASE.require(len(owned) == 8 and {p.name for p in owned} == EXPECTED_OWN, "Expected exact eight preexisting owned assets")
    BASE.require(len(protected) == 3962, "Expected exact3962 protected source/config/production/original-map inputs")
    BASE.require(len(saves) == 458, "Expected exact458 non-selected save files")
    BASE.require(not set(candidate).intersection(owned), "Candidate and owned backup inputs overlap")
    categories = {"candidate": candidate, "owned": owned, "protected": protected, "non_selected_saves": saves}
    pins = {name: [BASE.pin(p) for p in files] for name, files in categories.items()}
    build_pins = json.loads((ROOT / "Feature/doc/evidence/landscape-natural-boundary20261117/build/source-hashes-refinement.json").read_text())
    protected_index = {row["path"]: row for row in pins["protected"]}
    BASE.require(all(protected_index[row["path"]]["sha256"].lower() == row["sha256"].lower()
                     for row in build_pins["files"]), "Built native source hash changed")
    BASE.make_directory(BACKUP.parent)
    BASE.project_path(BACKUP).mkdir(exist_ok=False)
    copied = pins["candidate"] + pins["owned"]
    for row in copied:
        source = BASE.project_path(ROOT / row["path"])
        BASE.require(BASE.same_content(BASE.pin(source), row), "Source changed before copy: " + row["path"])
        dest = BASE.project_path(BACKUP / row["path"])
        BASE.require(dest.is_relative_to(BACKUP), "Copy destination escaped leaf")
        BASE.make_directory(dest.parent)
        with source.open("rb") as incoming, dest.open("xb") as outgoing:
            while block := incoming.read(BASE.CHUNK_BYTES):
                outgoing.write(block)
        readback = BASE.pin(dest)
        BASE.require(readback["size"] == row["size"] and readback["sha256"] == row["sha256"], "Backup readback mismatch")
    current = enumerate_inputs()
    for name, new_files in zip(("candidate", "owned", "protected", "non_selected_saves"), current[:4]):
        BASE.require(set(new_files) == set(categories[name]), "Scoped inventory changed: " + name)
        for row in pins[name]:
            BASE.require(BASE.same_content(BASE.pin(ROOT / row["path"]), row), "Scoped file changed: " + row["path"])
    expected_copy_names = {row["path"] for row in copied}
    actual_copy_names = {p.relative_to(BACKUP).as_posix() for p in BASE.tree_files(BACKUP, required=True)}
    BASE.require(actual_copy_names == expected_copy_names, "Physical backup inventory differs from exact1481 inputs")
    report = {
        "schema": "Rogue10m.CaveDiskCheckpoint.20261118.v1",
        "status": "EXACT1481_PHYSICAL_BACKUP_SHA256_VERIFIED_SCOPED_INPUTS_UNCHANGED",
        "recorded_utc": datetime.now(timezone.utc).isoformat(), "project": str(ROOT),
        "reference_saved_map_commit": REFERENCE, "current_head": current_head,
        "tracked_source_candidate_and_owned_match_reference": True,
        "candidate_map": "/Game/" + BASE.CANDIDATE, "backup_root": BACKUP.relative_to(ROOT).as_posix(),
        "candidate_directories": candidate_dirs, "protected_directories": protected_dirs,
        "backed_up_files": pins["candidate"], "owned_backed_up_files": pins["owned"],
        "protected_files": pins["protected"], "non_selected_save_files": pins["non_selected_saves"],
        "selected_qa_save_files_excluded": [p.relative_to(ROOT).as_posix() for p in selected],
        "candidate_file_count": len(candidate), "candidate_bytes": sum(r["size"] for r in pins["candidate"]),
        "owned_file_count": len(owned), "owned_bytes": sum(r["size"] for r in pins["owned"]),
        "backed_up_file_count": len(copied), "backed_up_bytes": sum(r["size"] for r in copied),
        "protected_file_count": len(protected), "protected_bytes": sum(r["size"] for r in pins["protected"]),
        "non_selected_save_file_count": len(saves), "non_selected_save_bytes": sum(r["size"] for r in pins["non_selected_saves"]),
        "fingerprints": {name: fingerprint(rows) for name, rows in pins.items()},
        "no_reparse_paths_verified": True, "built_native_sources_match_successful_build": True,
        "asset_mutations": 0, "saved_file_mutations": 0, "editor_operations": 0, "git_mutations": 0,
        "limitations": ["Disk checkpoint only; unsaved editor state is not inspected or certified.",
                        "Only candidate and eight owned assets are physically copied; other declared inputs are hashed.",
                        "Selected QA save files are excluded from the preservation contract."]}
    BASE.make_directory(MANIFEST.parent)
    with BASE.project_path(MANIFEST).open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(report, stream, ensure_ascii=False, indent=2)
        stream.write("\n")
    print(json.dumps({k: report[k] for k in ("status", "current_head", "candidate_file_count", "candidate_bytes",
          "owned_file_count", "owned_bytes", "backed_up_file_count", "backed_up_bytes", "protected_file_count",
          "non_selected_save_file_count", "backup_root", "fingerprints")}, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
