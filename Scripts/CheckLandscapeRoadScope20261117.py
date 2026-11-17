"""Read-only final disk scope audit against the checkpoint and native cold proof."""
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RESUME = ROOT / "Feature/doc/evidence/landscape-resume-20261117"
ROAD = ROOT / "Feature/doc/evidence/landscape-road-surface20261117"
TARGET = "Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A"
FOLDER = "Content/__ExternalObjects__/" + TARGET + "/4/U0/7LSR0EPX24RNQB6FXJ4SNF.uasset"
MATERIAL = "Content/Rogue10m/World/Production/WorldLayoutCompactCrop20261114A/RoadSurface20261117/M_RoadSurface.uasset"


def read(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def row(path):
    return {"path":path.relative_to(ROOT).as_posix(),"size":path.stat().st_size,
            "sha256":hashlib.sha256(path.read_bytes()).hexdigest().upper()}


def content_path(package):
    assert package.startswith("/Game/__ExternalActors__/" + TARGET + "/")
    return "Content/" + package[6:] + ".uasset"


def main():
    assert ROOT == Path("D:/Project/Rogue10m")
    dest=RESUME/"final-scope-report.json"
    assert not dest.exists(), "Preserve earlier scope evidence"
    base=read(RESUME/"baseline-files.json")
    cold=read(ROAD/"final-cold/comparison.json")
    assert cold["passed"] is True and all(cold["checks"].values()), "Strict cold validation must pass first"
    before={r["path"]:r for r in base["backed_up_files"]}
    current={}
    roots=[ROOT/d["path"] for d in base["candidate_directories"]]
    roots.append(ROOT/("Content/"+TARGET+".umap"))
    for directory in roots:
        for path in directory.rglob("*") if directory.is_dir() else [directory]:
            if path.is_file(): current[path.relative_to(ROOT).as_posix()]=row(path)
    changed={n for n in before.keys() & current.keys() if before[n]["sha256"].upper()!=current[n]["sha256"]}
    added=current.keys()-before.keys()
    removed=before.keys()-current.keys()
    source={content_path(p) for p in cold["source_packages"].values()}
    new={content_path(p["package"]) for p in cold["projectors"]}
    protected_changes=[]
    for old in base["protected_files"]:
        path=ROOT/old["path"]
        if not path.is_file() or row(path)["sha256"]!=old["sha256"].upper():
            protected_changes.append(old["path"])
    checks={"exact663_source_packages":len(source)==663,
            "exact29_decal_packages":len(new)==29,
            "changed_exactly_source_visibility_packages":changed==source,
            "added_exactly29_decals_and_native_folder":added==new|{FOLDER},
            "no_candidate_files_removed":not removed,
            "candidate_count1453":len(current)==1453,
            "protected3951_unchanged":len(base["protected_files"])==3951 and not protected_changes,
            "owned_material_saved":(ROOT/MATERIAL).is_file(),
            "cold_all_checks_passed":cold["passed"] is True}
    report={"utc":datetime.now(timezone.utc).isoformat(),"passed":all(checks.values()),"checks":checks,
            "baseline_candidate_count":len(before),"current_candidate_count":len(current),
            "changed":sorted(changed),"added":sorted(added),"removed":sorted(removed),
            "owned_material":row(ROOT/MATERIAL),"protected_changes":protected_changes,
            "protected_count":len(base["protected_files"]),"cold_comparison":str(ROAD/"final-cold/comparison.json"),
            "cold_protected_saves":cold["protected_saves"],
            "scope":"Candidate map/EA/EO and specified baseline protected files only; no whole-repository/global dependency claim."}
    with dest.open("x",encoding="utf-8") as stream: json.dump(report,stream,ensure_ascii=False,indent=2)
    print(json.dumps({"passed":report["passed"],"checks":checks,"changed":len(changed),"added":len(added),"removed":len(removed)},ensure_ascii=False))
    return int(not report["passed"])


if __name__=="__main__":
    raise SystemExit(main())
