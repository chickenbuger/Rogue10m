"""Read-only saved-file scope audit after successful independent cold QA.

No Unreal imports, editor/process control, Git mutation or asset writes. main()
requires cold-live/result.json passed before hashing the final candidate and
writes only a fresh JSON evidence report. Existing evidence is never replaced.
"""
from __future__ import annotations

import argparse
from collections import defaultdict
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import stat


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "Feature/doc/evidence/landscape-natural-boundary20261117"
MAP = "Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A"
TARGET = "/Game/" + MAP
EA = "Content/__ExternalActors__/" + MAP + "/"
EO = "Content/__ExternalObjects__/" + MAP + "/"
MAP_FILE = "Content/" + MAP + ".umap"
OWN = "Content/Rogue10m/World/Production/WorldLayoutCompactCrop20261114A/NaturalBoundary20261117/"
MATERIAL_DERIVATIVES = {"/Game/Rogue10m/World/StartVillage/Materials/M_StartVillage_"+name:
    "/Game/"+OWN[8:]+"M_Natural_"+name for name in ("DarkTimber", "WarmStone", "Banner_Teal", "Banner_Ochre", "Banner_Blue")}
OWN_ASSETS = {OWN + "M_RoadFeather.uasset", OWN + "DA_IntroSupply_Flat.uasset"} | {
    "Content/"+package[6:]+".uasset" for package in MATERIAL_DERIVATIVES.values()}
BUILD_CS = "Source/Rogue10m/Rogue10m.Build.cs"
NATIVE_SOURCES = {"Source/Rogue10m/Editor/Rogue10mNaturalBoundary20261117Authoring." + suffix for suffix in ("h", "cpp")}
METADATA_CLASSES = {"/Script/CoreUObject.Package", "/Script/CoreUObject.MetaData"}
EO_CLASSES = {"/Script/Engine.Texture2D", "/Script/Engine.ActorFolder"}
REPARSE = getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400)


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def safe(value):
    p = Path(value)
    p = p if p.is_absolute() else ROOT / p
    p = Path(os.path.abspath(p))
    require(p.is_relative_to(ROOT), "Path outside the exact project: " + str(p))
    for current in reversed((p, *p.parents)):
        try:
            info = current.lstat()
        except FileNotFoundError:
            continue
        require(not stat.S_ISLNK(info.st_mode) and not getattr(info, "st_file_attributes", 0) & REPARSE,
                "Symlink/reparse path is forbidden: " + str(current))
    return p


def read(value):
    return json.loads(safe(value).read_text(encoding="utf-8-sig"))


def pin(value):
    p = safe(value)
    before = p.stat()
    require(stat.S_ISREG(before.st_mode), "Expected regular file: " + str(p))
    digest = hashlib.sha256()
    with p.open("rb") as stream:
        while data := stream.read(1024*1024):
            digest.update(data)
        require((before.st_size, before.st_mtime_ns) == (os.fstat(stream.fileno()).st_size,
                os.fstat(stream.fileno()).st_mtime_ns), "File changed while hashing: " + str(p))
    after = p.stat()
    require((before.st_size, before.st_mtime_ns) == (after.st_size, after.st_mtime_ns), "File changed during inspection: " + str(p))
    return {"path": p.relative_to(ROOT).as_posix(), "size": before.st_size, "sha256": digest.hexdigest()}


def files(directory):
    root = safe(directory)
    if not root.exists():
        return []
    require(root.is_dir(), "Expected directory: " + str(root))
    result, pending = [], [root]
    while pending:
        with os.scandir(safe(pending.pop())) as entries:
            for entry in entries:
                p = safe(entry.path)
                info = p.lstat()
                if stat.S_ISDIR(info.st_mode):
                    pending.append(p)
                else:
                    require(stat.S_ISREG(info.st_mode), "Special file is forbidden: " + str(p))
                    result.append(p.relative_to(ROOT).as_posix())
    return sorted(result)


def asset_file(package):
    require(package.startswith("/Game/"), "Only project packages may be classified: " + package)
    return "Content/" + package[6:] + ".uasset"


def file_package(relative):
    p = Path(relative)
    require(relative.startswith("Content/") and p.suffix.lower() in (".uasset", ".umap", ".uexp", ".ubulk"),
            "Unexpected candidate file type: " + relative)
    return "/Game/" + p.with_suffix("").as_posix()[8:]


def same(a, b):
    return a["size"] == b["size"] and a["sha256"].lower() == b["sha256"].lower()


def selected_qa_save(relative):
    name = Path(relative).name
    if name in ("Rogue10m_Town_QA_Landscape20261116A.sav", "Rogue10m_Fighter_QA_Landscape20261116A.sav"):
        return True
    return re.fullmatch(r"Rogue10m_Progression_QA_Landscape20261116A_AC920A0E4D79EBE2C1F3EEBE945FD67A(?:_v\d+)?\.sav", name) is not None


def build_report(*, cold_result=None, package_inventory=None, source_manifest=None):
    """Read scoped files only; the caller controls exclusive report creation."""
    cold_path = safe(cold_result or OUT / "cold-live/result.json")
    cold = read(cold_path)
    require(cold.get("passed") is True, "Successful independent cold-live/result.json is required before final scope audit")
    baseline = read(OUT / "baseline-files.json")
    plan = read(OUT / "author-plan.json")
    authored = read(OUT / "author-live/author-content.json")
    terrain = read(OUT / "baseline-live/terrain.json")
    baseline_actors = read(OUT / "baseline-live/actors.json")
    saved = read(OUT / "author-live/save-result.json")
    material_repair = read(OUT / "author-live/ism-material-repair.json")
    source_path = safe(source_manifest or OUT / "build/source-hashes-keyfix.json")
    source_pins = read(source_path)
    build_result = read(source_path.parent / source_pins["build_result"])
    inventory_path = safe(package_inventory or OUT / "cold-live/package-inventory.json")
    inventory = read(inventory_path)
    rows = inventory["packages"]
    if isinstance(rows, dict):
        rows = [dict(value, package=key) for key, value in rows.items()]
    packages = {r["package"]: r for r in rows}
    checks, violations = {}, []

    def check(name, condition, detail=None):
        checks[name] = bool(condition)
        if not condition:
            violations.append({"check": name, "detail": detail})

    report = {"schema": "Rogue10m.NaturalWorldFileScope.20261117.v1", "passed": False,
              "utc": datetime.now(timezone.utc).isoformat(), "checks": checks, "violations": violations,
              "cold_result": pin(cold_path), "package_inventory": pin(inventory_path),
              "source_manifest": pin(source_path), "asset_mutations": False, "unreal_calls": False}
    check("exact_baseline_and_plan", baseline["candidate_map"] == plan["map"] == TARGET
          and baseline["backed_up_file_count"] == 1453 and baseline["protected_file_count"] == 3952)
    check("native_bounded_save_success", saved.get("success") is True and saved.get("native_save_returned") is True
          and saved.get("save_all_dirty_called") is False)
    check("native_keyfix_build_passed", build_result.get("exitCode") == 0)
    requested = set(saved.get("requested_packages", []))
    actor_by_path = {a["path"]: a for a in baseline_actors}
    categories = {
        "vertical_moves": {asset_file(actor_by_path[r["path"]]["package"]) for r in plan["moves"]},
        "road_projection_and_material": {asset_file(r["package"]) for r in plan["road_decals"]},
        "quest_director_binding": {asset_file(actor_by_path[r["actor"]]["package"]) for r in plan["director_baseline"]},
        "landscape_actor_or_proxy": {asset_file(r["package"]) for r in terrain["proxies"]},
    }
    check("planned_actor_category_counts", [len(categories[k]) for k in categories] == [672, 29, 1, 17])
    new_actor_files = {asset_file(g["package"]) for g in authored["dressing"]["groups"]}
    check("declared_new_dressing_exact10", len(new_actor_files) == 10
          and {g["actor_label"] for g in authored["dressing"]["groups"]} == {g["actor_label"] for g in plan["dressing"]["groups"]})
    planned_folders = set()
    for group in plan["dressing"]["groups"]:
        parts = group["folder"].split("/")
        planned_folders.update("/".join(parts[:i]) for i in range(1, len(parts)+1))

    old = {r["path"]: r for r in baseline["backed_up_files"]}
    candidate_names = sorted({MAP_FILE, *files(EA), *files(EO)})
    current = {name: pin(name) for name in candidate_names}
    changed = sorted(p for p in set(old) & set(current) if not same(old[p], current[p]))
    added, removed = sorted(set(current)-set(old)), sorted(set(old)-set(current))
    report.update(baseline_candidate_count=len(old), current_candidate_count=len(current),
                  changed=changed, added=added, removed=removed, current_candidate_files=list(current.values()))
    check("no_candidate_removals", not removed, removed)
    check("new_actor_packages_exact10", {p for p in added if p.startswith(EA)} == new_actor_files,
          sorted({p for p in added if p.startswith(EA)} ^ new_actor_files))
    allowed_actor_files = set().union(*categories.values())
    unexplained = [p for p in changed if p != MAP_FILE and not p.startswith(EO) and p not in allowed_actor_files]
    unexplained += [p for p in added if not p.startswith(EO) and p not in new_actor_files]
    check("candidate_actor_changes_only_plan", not unexplained, unexplained)
    categorized = {name: {"planned_count": len(paths), "changed_count": len(paths & set(changed)),
                          "changed_files": sorted(paths & set(changed)), "byte_unchanged_files": sorted(paths & set(old)-set(changed))}
                   for name, paths in categories.items()}
    report["changed_actor_categories"] = categorized
    report["candidate_world_package_changed"] = MAP_FILE in changed

    eo_rows, unknown_eo, new_folders = [], [], []
    for relative in [p for p in changed+added if p.startswith(EO)]:
        pkg = file_package(relative)
        entry = packages.get(pkg)
        classes = set(entry.get("classes", [])) if entry else set()
        content_classes = classes - METADATA_CLASSES
        new = relative in added
        valid = bool(content_classes) and content_classes <= EO_CLASSES if new else (not content_classes or content_classes <= EO_CLASSES)
        if not valid:
            unknown_eo.append(relative)
        if new and "/Script/Engine.ActorFolder" in content_classes:
            new_folders.append(pkg)
        eo_rows.append({"path": relative, "package": pkg, "new": new, "classes": sorted(classes),
                        "cold_object_paths": entry.get("object_paths", []) if entry else [],
                        "directly_requested_save": pkg in requested,
                        "classification": "new_layer_texture_or_native_folder" if new else "exact_baseline_EO_metadata_or_terrain_update"})
    check("external_objects_narrow_classes", not unknown_eo, unknown_eo)
    check("new_native_folders_bounded_by_plan", len(set(new_folders)) <= len(planned_folders), new_folders)
    report.update(candidate_external_object_changes=eo_rows, new_native_folder_packages=sorted(set(new_folders)),
                  new_texture_packages=sorted({r["package"] for r in eo_rows if r["new"] and "/Script/Engine.Texture2D" in r["classes"]}),
                  planned_folder_paths=sorted(planned_folders))

    protected = {r["path"]: r for r in baseline["protected_files"]}
    protected_changed, protected_missing, accepted_build = [], [], None
    for relative, old_pin in protected.items():
        p = safe(relative)
        if not p.is_file():
            protected_missing.append(relative)
            continue
        now = pin(p)
        if same(old_pin, now):
            continue
        if relative == BUILD_CS:
            data = p.read_bytes()
            token = b', "Foliage"'
            normalized = data.replace(token, b"", 1)
            foliage = data.find(token)
            editor_if, includes = data.find(b"if (Target.bBuildEditor)"), data.find(b"PublicIncludePaths")
            only_foliage = (data.count(token) == 1 and 0 <= editor_if < foliage < includes
                and len(normalized) == old_pin["size"] and hashlib.sha256(normalized).hexdigest().lower() == old_pin["sha256"].lower())
            if only_foliage:
                accepted_build = {"path": relative, "actual": now, "baseline_sha256": old_pin["sha256"],
                                  "change": "Only editor-branch Foliage dependency insertion; byte reconstruction matches baseline"}
                continue
        protected_changed.append({"path": relative, "before": old_pin, "after": now})
    check("protected3952_only_editor_foliage_change", len(protected) == 3952 and not protected_changed
          and not protected_missing and accepted_build is not None,
          {"changed": protected_changed, "missing": protected_missing, "accepted_build": accepted_build})
    report.update(protected_count=len(protected), protected_byte_unchanged_count=len(protected)-len(protected_changed)-len(protected_missing)-(1 if accepted_build else 0),
                  protected_changes=protected_changed, protected_missing=protected_missing, accepted_build_dependency=accepted_build)
    protected_inventory = set()
    for row in baseline["protected_directories"]:
        protected_inventory.update(files(row["path"]))
    protected_inventory.update(p for p in protected if p.endswith(".umap"))
    protected_added = sorted(protected_inventory-set(protected))
    check("protected_inventory_only_two_sources_seven_owned_assets", set(protected_added) == NATIVE_SOURCES | OWN_ASSETS,
          {"actual_added": protected_added, "allowed": sorted(NATIVE_SOURCES | OWN_ASSETS)})
    report["protected_inventory_added"] = protected_added
    check("owned_namespace_exact_seven_assets", set(files(OWN)) == OWN_ASSETS, files(OWN))
    report["owned_assets"] = [pin(p) for p in sorted(OWN_ASSETS) if safe(p).is_file()]
    source_material_rows, bad_source_materials = [], []
    material_rows = material_repair.get("materials", [])
    check("ISM_source_material_receipt_exact5", material_repair.get("schema") == "Rogue10m.NaturalDressingMaterialDerivatives20261117.v1"
          and material_repair.get("success") is True and len(material_rows) == 5
          and {r.get("source_package") for r in material_rows} == set(MATERIAL_DERIVATIVES)
          and {r.get("destination_package") for r in material_rows} == set(MATERIAL_DERIVATIVES.values()))
    for source_package, destination in MATERIAL_DERIVATIVES.items():
        receipts = [r for r in material_rows if r.get("source_package") == source_package]
        relative = asset_file(source_package)
        now = pin(relative) if safe(relative).is_file() else None
        row = receipts[0] if len(receipts) == 1 else {}
        valid = (now is not None and row.get("destination_package") == destination
                 and row.get("source_sha256_before") == row.get("source_sha256_after")
                 and now["sha256"].lower() == str(row.get("source_sha256_before", "")).lower()
                 and row.get("source_dirty_after_reload") is False)
        source_material_rows.append({"source": source_package, "destination": destination,
            "current_file": now, "receipt_sha256_before": row.get("source_sha256_before"),
            "receipt_sha256_after": row.get("source_sha256_after"), "passed": bool(valid)})
        if not valid:
            bad_source_materials.append(relative)
    check("five_original_ISM_source_material_files_unchanged", not bad_source_materials, bad_source_materials)
    report["additional_original_ISM_material_files"] = source_material_rows
    source_expected = {r["path"]: r for r in source_pins["files"]}
    check("built_native_source_manifest_exact", set(source_expected) == NATIVE_SOURCES | {BUILD_CS})
    source_current, source_bad = [], []
    for relative in sorted(NATIVE_SOURCES | {BUILD_CS}):
        if not safe(relative).is_file():
            source_bad.append(relative)
            continue
        now = pin(relative)
        source_current.append(now)
        if relative not in source_expected or now["sha256"].lower() != source_expected[relative]["sha256"].lower():
            source_bad.append(relative)
    check("native_sources_unchanged_since_successful_build", not source_bad, source_bad)
    report["declared_built_sources"] = source_current

    saves = read(OUT / "protected-save-files.json")
    expected_saves = {r["path"]: r for r in saves["files"]}
    current_saves = {p for p in files("Saved/SaveGames") if p.endswith(".sav") and not selected_qa_save(p)}
    save_changed, save_missing = [], []
    for relative, expected in expected_saves.items():
        if not safe(relative).is_file():
            save_missing.append(relative)
        elif not same(pin(relative), expected):
            save_changed.append(relative)
    check("protected458_save_hashes", len(expected_saves) == saves["count"] == 458 and not save_changed and not save_missing,
          {"changed": save_changed, "missing": save_missing})
    check("non_selected_QA_save_inventory", current_saves == set(expected_saves),
          {"new": sorted(current_saves-set(expected_saves)), "missing": sorted(set(expected_saves)-current_saves)})
    report["protected_saves"] = {"count": len(expected_saves), "changed": save_changed, "missing": save_missing,
        "new": sorted(current_saves-set(expected_saves)), "allowed_changed_names": [p for p in files("Saved/SaveGames") if selected_qa_save(p)]}
    report.update(passed=all(checks.values()), scope="Exact candidate map/EA/EO, baseline protected3952 inputs plus declared additions, five original ISM material files, and non-selected-QA458 saves.",
                  limitations=["No whole-project, generated/cache, arbitrary other map or all-vendor filesystem audit.",
                    "Existing candidate EO updates use the exact baseline list and independent cold semantic QA; new EO require cold Texture2D/ActorFolder class evidence. Core Package/MetaData are metadata.",
                    "Native package-save receipts and cold proofs complement file hashes; file hashes alone do not certify visual, collision or gameplay correctness."])
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cold-result", type=Path, default=OUT / "cold-live/result.json")
    parser.add_argument("--package-inventory", type=Path, default=OUT / "cold-live/package-inventory.json")
    parser.add_argument("--source-manifest", type=Path, default=OUT / "build/source-hashes-keyfix.json")
    parser.add_argument("--output", type=Path, default=OUT / "final-scope-report.json")
    args = parser.parse_args()
    try:
        report = build_report(cold_result=args.cold_result, package_inventory=args.package_inventory,
                              source_manifest=args.source_manifest)
        destination = safe(args.output)
        require(destination.is_relative_to(OUT), "Evidence output must stay in this feature folder")
        with destination.open("x", encoding="utf-8") as stream:
            json.dump(report, stream, ensure_ascii=False, indent=2)
            stream.write("\n")
        print(json.dumps({"passed": report["passed"], "checks": report["checks"], "report": str(destination)}, ensure_ascii=False))
        return 0 if report["passed"] else 1
    except (OSError, ValueError, RuntimeError, KeyError, TypeError) as error:
        print(json.dumps({"passed": False, "error": str(error)}, ensure_ascii=False))
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
