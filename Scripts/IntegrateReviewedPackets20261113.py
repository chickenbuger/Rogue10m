"""Install the two reviewed source packets with exact preimages and physical backups.

No branch/index mutation, asset writes, build launch, or GUI input. Default is plan only.
"""
from __future__ import annotations

import argparse
import datetime
import hashlib
import json
from pathlib import Path
import re
import stat
import subprocess


ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / "Feature/doc/evidence/map-resume-20261113"
COMBINED = ROOT / "Feature/implementation/map-resume-20261113/combined-g08-vfx/run-20261113-b/Delivery"
NPC = Path("C:/Users/PC/Documents/Codex/2026-11-11/task/NpcVillageQuestWork")


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest() if path.is_file() else None


def git(*args):
    result = subprocess.run(["git", "-c", "core.autocrlf=true", *args], cwd=ROOT,
                            capture_output=True, text=True, encoding="utf-8", errors="replace")
    if result.returncode:
        raise RuntimeError(f"git {args[0]} failed: {result.stdout}\n{result.stderr}")
    return result.stdout


def safe_path(relative):
    path = ROOT / relative
    assert not Path(relative).is_absolute() and ".." not in Path(relative).parts
    assert path.resolve().is_relative_to(ROOT.resolve())
    for ancestor in (path, *path.parents):
        if ancestor == ROOT.parent:
            break
        if ancestor.exists() or ancestor.is_symlink():
            attributes = getattr(ancestor.lstat(), "st_file_attributes", 0)
            assert not attributes & stat.FILE_ATTRIBUTE_REPARSE_POINT, f"Reparse path: {ancestor}"
    return path


def patch_paths(path):
    pairs = re.findall(r"^diff --git a/(\S+) b/(\S+)$", path.read_text(encoding="utf-8"), re.M)
    assert pairs and all(a == b for a, b in pairs), "Only same-path text changes accepted"
    paths = [a for a, _ in pairs]
    assert len(paths) == len(set(paths))
    for relative in paths:
        assert relative.startswith(("Source/", "Scripts/", "Docs/", "Feature/")), relative
        safe_path(relative)
    return paths


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--execute", action="store_true")
    args = parser.parse_args()
    assert git("branch", "--show-current").strip() == "Sprint#9-32-landscape-world-blockout"
    combined_patch = COMBINED / "combined-g08-vfx-full-20261113.patch"
    npc_patch = NPC / "Integration/village-quests-intro-source.patch"
    assert sha(combined_patch) == "7175e6f0e54250f79720f0864d883c2213513c38ddcc95612f54880a30ac035b"
    assert sha(npc_patch) == "4f8f131030365171d41175575b27c1297ab0133e53ff576f64fc05a1d6f0e7b6"
    combined_rows = json.loads((COMBINED / "combined-manifest.json").read_text(encoding="utf-8"))["entries"]
    npc_manifest = json.loads((NPC / "Integration/intro-verification.json").read_text(encoding="utf-8"))
    npc_rows = {r["path"]: r for r in npc_manifest["deliverable_files"]}
    packets = [(combined_patch, patch_paths(combined_patch)), (npc_patch, patch_paths(npc_patch))]
    assert [len(paths) for _, paths in packets] == [29, 30]
    touched = set(packets[0][1]) | set(packets[1][1])
    assert len(touched) == 59, "Unexpected overlapping packets"
    baseline = json.loads((BASE / "BaselineA/checkpoint.json").read_text(encoding="utf-8"))
    for row in baseline["files"]:
        assert sha(ROOT / row["path"]) == row["sha256"].lower(), f"Baseline drift: {row['path']}"
    for row in combined_rows:
        assert sha(safe_path(row["path"])) == row["base_sha256"], f"Combined preimage drift: {row['path']}"
    hooks = {
        "Source/Rogue10m/UI/Widgets/Rogue10mTutorialWidget.cpp": "1b9b7ad68a264d6ed3d24ffdbd2a98d841ef602f",
        "Source/Rogue10m/World/Rogue10mTutorialNPC.cpp": "9e6dc5df6f406cc50a8360b7dbf184dd1e56ad0f",
        "Source/Rogue10m/World/Rogue10mTutorialNPC.h": "d929470e2bce22f60557f11b2ede5cec0722f27f",
    }
    for relative in packets[1][1]:
        path = safe_path(relative)
        if relative in hooks:
            assert git("hash-object", "--path=" + relative, relative).strip() == hooks[relative]
        else:
            assert not path.exists(), f"NPC add target already exists: {relative}"
        assert relative in npc_rows, f"NPC postimage missing: {relative}"
    for patch, _ in packets:
        git("apply", "--check", str(patch))
    result = {"status": "plan_verified", "utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
              "packets": [{"patch": str(p), "sha256": sha(p), "files": paths} for p, paths in packets],
              "baseline_files_verified": len(baseline["files"]), "touched_count": len(touched)}
    if not args.execute:
        print(json.dumps(result, ensure_ascii=False, indent=2))
        return
    out = BASE / "PacketIntegrationA"
    out.mkdir(exist_ok=False)
    result["tracked_status_before"] = git("status", "--short", "--untracked-files=no")
    result["preimages"] = []
    for relative in sorted(touched):
        path = safe_path(relative)
        result["preimages"].append({"path": relative, "sha256": sha(path), "existed": path.exists()})
        if path.is_file():
            backup = out / "backup" / relative
            backup.parent.mkdir(parents=True, exist_ok=True)
            backup.write_bytes(path.read_bytes())
            assert sha(backup) == sha(path)
    (out / "preflight.json").write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    applied = []
    for patch, _ in packets:
        git("apply", "--check", str(patch))
        git("apply", str(patch))
        applied.append(str(patch))
        (out / "applied.json").write_text(json.dumps(applied, indent=2), encoding="utf-8")
    for row in combined_rows:
        assert sha(safe_path(row["path"])) == row["candidate_sha256"], f"Combined postimage mismatch: {row['path']}"
    for relative in packets[1][1]:
        assert git("hash-object", "--path=" + relative, relative).strip() == npc_rows[relative]["normalized_git_blob"], relative
    for row in baseline["files"]:
        if row["path"] not in touched:
            assert sha(ROOT / row["path"]) == row["sha256"].lower(), f"Unrelated file drift: {row['path']}"
    result.update(status="source_packets_installed_compile_pending", applied=applied,
                  combined_raw_postimages_verified=len(combined_rows), npc_normalized_postimages_verified=30,
                  baseline_untouched_files_verified=sum(row["path"] not in touched for row in baseline["files"]),
                  gui_input_operations=0, assets_modified=False, index_or_branch_mutation=False)
    (out / "result.json").write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps({k: v for k, v in result.items() if k not in ("packets", "preimages", "tracked_status_before")}, indent=2))


if __name__ == "__main__":
    main()
