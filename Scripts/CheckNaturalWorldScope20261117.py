"""Read-only saved-file scope audit after successful independent cold QA.

No Unreal imports, editor/process control, Git mutation or asset writes. main()
requires successful independent cold QA before hashing the final candidate and
writes only a fresh JSON evidence report. The refinement profile additionally
requires complete-stage structural QA and explicit fresh save/source receipts.
The original 1,453-file baseline remains the comparison boundary in both modes.
Existing evidence is never replaced.
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
REFINEMENT_ASSETS = OWN_ASSETS | {OWN + "M_VillageEarth.uasset"}
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


def build_report(*, profile="baseline", cold_result=None, cold_validation=None,
                 package_inventory=None, source_manifest=None, author_plan=None,
                 authored_content=None, save_result=None, material_repair=None,
                 boss_plan=None, boss_author=None, floor_plan=None, floor_author=None):
    """Read scoped files only; the caller controls exclusive report creation."""
    require(profile in ("baseline", "refinement"), "Unknown scope profile")
    refinement = profile == "refinement"
    if refinement:
        # Exact new-stage defaults; never fall back to the old checkpoint's
        # successful save/cold evidence when the new receipt is absent.
        cold_result = cold_result or OUT / "refinement-cold/result.json"
        package_inventory = package_inventory or OUT / "refinement-cold/package-inventory.json"
        source_manifest = source_manifest or OUT / "build/source-hashes-refinement.json"
        save_result = save_result or OUT / "refinement-live/final-save-result.json"
        boss_author = boss_author or OUT / "refinement-live/boss-threshold.json"
        floor_author = floor_author or OUT / "refinement-live/village-earth.json"
    owned_assets = REFINEMENT_ASSETS if refinement else OWN_ASSETS
    cold_path = safe(cold_result or OUT / "cold-live/result.json")
    cold = read(cold_path)
    require(cold.get("passed") is True, "Successful independent cold result is required before final scope audit")
    baseline = read(OUT / "baseline-files.json")
    plan_path = safe(author_plan or OUT / ("author-plan-gate-reflow.json" if refinement else "author-plan.json"))
    authored_path = safe(authored_content or OUT / "author-live/author-content.json")
    plan, authored = read(plan_path), read(authored_path)
    terrain = read(OUT / "baseline-live/terrain.json")
    baseline_actors = read(OUT / "baseline-live/actors.json")
    saved_path = safe(save_result or OUT / "author-live/save-result.json")
    saved = read(saved_path)
    repair_path = safe(material_repair or OUT / "author-live/ism-material-repair.json")
    material_repair = read(repair_path)
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

    report = {"schema": "Rogue10m.NaturalWorldFileScope.20261117.v2" if refinement else "Rogue10m.NaturalWorldFileScope.20261117.v1",
              "profile": profile, "passed": False,
              "utc": datetime.now(timezone.utc).isoformat(), "checks": checks, "violations": violations,
              "cold_result": pin(cold_path), "package_inventory": pin(inventory_path),
              "source_manifest": pin(source_path), "author_plan": pin(plan_path),
              "authored_content": pin(authored_path), "save_result": pin(saved_path),
              "material_repair": pin(repair_path), "asset_mutations": False, "unreal_calls": False}
    check("exact_baseline_and_plan", baseline["candidate_map"] == plan["map"] == TARGET
          and baseline["backed_up_file_count"] == 1453 and baseline["protected_file_count"] == 3952)
    check("native_bounded_save_success", saved.get("success") is True and saved.get("native_save_returned") is True
          and saved.get("save_all_dirty_called") is False)
    check("native_refinement_build_passed" if refinement else "native_keyfix_build_passed", build_result.get("exitCode") == 0)
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

    if refinement:
        fresh_path = safe(OUT / "refinement-baseline-files.json")
        fresh = read(fresh_path)
        fresh_candidate_rows = fresh.get("backed_up_files", [])
        fresh_protected_rows = fresh.get("protected_files", [])
        fresh_candidate = {row["path"]: row for row in fresh_candidate_rows}
        fresh_protected = {row["path"]: row for row in fresh_protected_rows}
        checkpoint_valid = (fresh.get("status") == "CANDIDATE_PHYSICAL_BACKUP_SHA256_VERIFIED_PROTECTED_INPUTS_UNCHANGED"
            and fresh.get("candidate_map") == TARGET and fresh.get("backed_up_file_count") == 1468
            and fresh.get("protected_file_count") == 3959 and len(fresh_candidate_rows) == len(fresh_candidate) == 1468
            and len(fresh_protected_rows) == len(fresh_protected) == 3959
            and fresh.get("no_reparse_paths_verified") is True and fresh.get("source_inputs_changed") == []
            and all(fresh.get(key) == 0 for key in ("vendors_or_previous_evidence_edited",
                "unreal_processes_launched", "asset_mutations", "deletions")))
        check("fresh_refinement_checkpoint_exact1468_protected3959", checkpoint_valid)
        require(checkpoint_valid, "Exact physical refinement checkpoint is required; do not substitute the old 1453-file baseline")
        backup_root = safe(fresh["backup_root"])
        require(backup_root.is_dir(), "Fresh physical backup directory is absent")
        owned_backup_path = safe(OUT / "refinement-owned-assets.json")
        owned_backup_receipt = read(owned_backup_path)
        owned_backup_rows = owned_backup_receipt.get("backed_up", [])
        owned_backup = {row["path"]: row for row in owned_backup_rows}
        owned_backup_valid = (len(owned_backup_rows) == len(owned_backup) == 7
            and set(owned_backup) == OWN_ASSETS and not set(owned_backup) & set(fresh_candidate)
            and all(relative in fresh_protected and same(row, fresh_protected[relative])
                    for relative, row in owned_backup.items()))
        check("exact_seven_owned_backup_manifest_matches_fresh_protected", owned_backup_valid)
        require(owned_backup_valid, "Separate immutable seven-owned-asset backup receipt must match fresh protected hashes")
        # The two retained manifests describe distinct, intentional copies in
        # the same physical directory: 1,468 candidate files and seven OWN files.
        expected_backup = dict(fresh_candidate)
        expected_backup.update(owned_backup)
        backup_prefix = backup_root.relative_to(ROOT).as_posix()+"/"
        expected_backup_names = {backup_prefix+relative for relative in expected_backup}
        actual_backup_names = set(files(backup_root))
        backup_changed, backup_missing = [], []
        for relative, expected in expected_backup.items():
            copy = safe(backup_root / relative)
            require(copy.is_relative_to(backup_root), "Backup entry escapes the physical checkpoint")
            if not copy.is_file():
                backup_missing.append(relative)
            elif not same(expected, pin(copy)):
                backup_changed.append(relative)
        check("fresh_physical_backup1475_hashes_preserved", len(expected_backup) == 1475 and actual_backup_names == expected_backup_names
              and not backup_changed and not backup_missing,
              {"changed": backup_changed, "missing": backup_missing,
               "unexpected": sorted(actual_backup_names-expected_backup_names)})
        fresh_owned = {relative: row for relative, row in fresh_protected.items() if relative.startswith(OWN)}
        old_owned_rows, old_owned_bad = [], []
        for relative in sorted(OWN_ASSETS):
            current_owned = pin(relative) if safe(relative).is_file() else None
            expected_owned = fresh_owned.get(relative)
            valid = current_owned is not None and expected_owned is not None and same(current_owned, expected_owned)
            old_owned_rows.append({"path": relative, "before": expected_owned,
                                  "after": current_owned, "passed": bool(valid)})
            if not valid:
                old_owned_bad.append(relative)
        check("all_seven_preexisting_owned_assets_byte_preserved", set(fresh_owned) == OWN_ASSETS
              and len(fresh_owned) == 7 and not old_owned_bad,
              {"mismatched": old_owned_bad, "fresh_owned_paths": sorted(fresh_owned)})
        report["refinement_fresh_baseline"] = pin(fresh_path)
        report["refinement_owned_backup_manifest"] = pin(owned_backup_path)
        report["owned_seven_preservation"] = {"checked": 7, "rows": old_owned_rows,
            "passed": checks["all_seven_preexisting_owned_assets_byte_preserved"],
            "only_allowed_new_owned_asset": OWN+"M_VillageEarth.uasset"}
        report["fresh_physical_backup"] = {"root": backup_root.relative_to(ROOT).as_posix(),
            "checked": len(expected_backup), "candidate_checked": len(fresh_candidate), "owned_checked": len(owned_backup),
            "changed": backup_changed, "missing": backup_missing,
            "unexpected": sorted(actual_backup_names-expected_backup_names),
            "passed": checks["fresh_physical_backup1475_hashes_preserved"]}
        validation_path = safe(cold_validation or cold_path.parent / "validation.json")
        validation = read(validation_path)
        boss_plan_path = safe(boss_plan or OUT / "boss-threshold-plan20261118B.json")
        floor_plan_path = safe(floor_plan or OUT / "village-earth-plan20261118.json")
        boss_path, floor_path = safe(boss_author), safe(floor_author)
        boss, floors = read(boss_path), read(floor_path)
        boss_recipe, floor_recipe = read(boss_plan_path), read(floor_plan_path)
        report["refinement_receipts"] = {"cold_validation": pin(validation_path),
            "boss_plan": pin(boss_plan_path), "boss_author": pin(boss_path),
            "floor_plan": pin(floor_plan_path), "floor_author": pin(floor_path)}
        cold_checks = validation.get("checks", {})
        required_cold = {"unique_actor_paths", "all_original_actors_preserved", "original_actor_exact_allowed_changes",
            "new_actors_exact_declared_stage", "exact_declared_instance_transforms_and_settings",
            "boss_threshold_actual_inner_bounds", "village_earth_exact16_node_classes",
            "village_earth_palette_and_source", "village_earth_circle_opacity_graph",
            "village_earth_three_exact_shallow_visual_decals", "read_only_dirty_set", "saved_clean_state",
            "native_exact45_registered_and_settled", "native_read_only", "full_target_grid_exact",
            "independent_target_SHA256", "original_layer_native_hash_preserved",
            "original_raw_RG_paint_preserved", "original_raw_BA_preserved", "strict_loaded_reference_walk",
            "dependency_limits_retained", "group_identity_BossThreshold", "group_components_BossThreshold"}
        check("independent_refinement_cold_complete", validation.get("schema") == "Rogue10m.NaturalWorldStructuralQA.20261117.v1"
              and validation.get("passed") is True and validation.get("refinement_stage") == "complete"
              and isinstance(cold_checks, dict) and bool(cold_checks)
              and required_cold <= set(cold_checks) and all(value is True for value in cold_checks.values())
              and validation.get("assets_saved") is False and validation.get("editor_mutated") is False
              and validation.get("dirty_after") == [] and not validation.get("diagnostics")
              and not validation.get("traceback") and cold.get("checks") == len(cold_checks))
        native = validation.get("native", {})
        grid = native.get("composite_heights_uint16", [])
        check("refinement_native45_two_layers_full179488_grid", native.get("success") is True
              and native.get("map") == TARGET and native.get("component_count") == native.get("collision_count") == 45
              and native.get("layer_count") == 2 and (native.get("width"), native.get("height")) == (568, 316)
              and isinstance(grid, list) and len(grid) == 179488
              and all(isinstance(value, int) and not isinstance(value, bool) and 0 <= value <= 65535 for value in grid)
              and native.get("landscape_reports_up_to_date") is True and native.get("read_only") is True
              and native.get("dirty_package_names_unchanged") is True and native.get("dirty_packages") == [])
        components = validation.get("dressing_components", [])
        original_paths = {row["path"] for row in baseline_actors}
        expected_ids = {group["id"] for group in plan["dressing"]["groups"]} | {"BossThreshold"}
        check("refinement1404_actors713_instances", len(baseline_actors) == len(original_paths) == plan.get("base_actor_count") == 1390
              and len(components) == 24 and {row.get("group") for row in components} == expected_ids
              and all(isinstance(row.get("count"), int) and row["count"] > 0 for row in components)
              and sum(row["count"] for row in components) == 713
              and sum(row["count"] for row in components if row.get("group") == "BossThreshold") == 5)
        boss_actor = boss.get("actor", {})
        check("additive_boss_receipt_preserves_all1400_old_flags", boss.get("schema") == "Rogue10m.NaturalBossThresholdAuthor.20261118.v1"
              and boss.get("success") is True and boss.get("baseline_actor_count") == 1400
              and boss.get("actor_count_after") == 1401 and boss.get("instance_count") == 5
              and boss.get("existing_actor_changes") == [] and boss.get("landscape_mutated") is False
              and boss.get("materials_created") == [] and boss.get("assets_saved") is False
              and boss.get("existing1400_actor_snapshots_equal") is True
              and bool(boss.get("existing_actor_snapshot_sha256_before"))
              and boss.get("existing_actor_snapshot_sha256_before") == boss.get("existing_actor_snapshot_sha256_after")
              and bool(boss.get("boss_ground_before")) and boss.get("boss_ground_before") == boss.get("boss_ground_after")
              and boss_recipe.get("schema") == "Rogue10m.NaturalBossThresholdPlan.20261118.v1"
              and boss_recipe.get("revision") == "B" and boss_recipe.get("map") == TARGET
              and boss_recipe.get("boss_ground", {}).get("all_properties_preserved") is True
              and boss.get("plan_sha256", "").lower() == pin(boss_plan_path)["sha256"].lower()
              and boss_actor.get("label") == boss_recipe.get("actor_label") == "NW18_BossThreshold"
              and boss_actor.get("folder") == boss_recipe.get("folder") == "NaturalWorld20261117/BossThreshold"
              and boss_actor.get("class") == "/Script/Engine.Actor"
              and boss.get("created_actor_paths") == [boss_actor.get("path")]
              and boss.get("packages") == [boss_actor.get("package")])
        floor_actors = floors.get("actors", [])
        floor_labels = {"NW18_VillageEarth_"+hub for hub in ("H", "M", "T")}
        check("additive_three_floor_receipt_preserves_all1401_old_actors", floors.get("schema") == "Rogue10m.VillageEarthFloorAuthor.20261118.v1"
              and floors.get("success") is True and floors.get("actor_count_after") == 1404
              and floors.get("existing1401_actor_snapshots_equal") is True
              and bool(floors.get("existing_actor_snapshot_sha256_before"))
              and floors.get("existing_actor_snapshot_sha256_before") == floors.get("existing_actor_snapshot_sha256_after")
              and floors.get("existing_actor_changes") == [] and floors.get("terrain_mutated") is False
              and floors.get("source_materials_mutated") == [] and floors.get("assets_saved") is False
              and floors.get("save_called") is False and floors.get("native_height_hashes_equal") is True
              and floor_recipe.get("schema") == "Rogue10m.VillageEarthFloor.20261118.v1" and floor_recipe.get("map") == TARGET
              and floor_recipe.get("material") == "/Game/"+OWN[8:]+"M_VillageEarth"
              and floor_recipe.get("folder") == "NaturalWorld20261117/VillageIdentity"
              and floors.get("material", {}).get("package") == floor_recipe.get("material")
              and floors.get("material", {}).get("node_count") == 16
              and len(floor_actors) == 3 and {row.get("label") for row in floor_actors} == floor_labels
              and all(row.get("folder") == floor_recipe["folder"] and row.get("class") == "/Script/Engine.DecalActor" for row in floor_actors)
              and set(floors.get("created_actor_paths", [])) == {row.get("path") for row in floor_actors})
        boss_file = asset_file(boss_actor["package"])
        floor_files = {asset_file(row["package"]) for row in floor_actors}
        check("four_refinement_packages_disjoint_from_ten_dressing", len(floor_files) == 3
              and boss_file not in floor_files and not ({boss_file} | floor_files) & new_actor_files
              and all(path.startswith(EA) for path in {boss_file} | floor_files))
        required_final_save = {boss_actor["package"], floor_recipe["material"]} | {row["package"] for row in floor_actors}
        check("fresh_final_save_contains_all_four_refinement_actors_and_material",
              saved_path != safe(OUT / "author-live/save-result.json") and required_final_save <= requested,
              {"required": sorted(required_final_save), "unrequested": sorted(required_final_save-requested)})
        new_actor_files |= {boss_file} | floor_files
        planned_folders.add("NaturalWorld20261117/BossThreshold")
        check("exact_six_planned_folder_paths", planned_folders == {"NaturalWorld20261117",
              "NaturalWorld20261117/BoundaryCollision", "NaturalWorld20261117/OptionalNarrative",
              "NaturalWorld20261117/VillageEnclosure", "NaturalWorld20261117/VillageIdentity",
              "NaturalWorld20261117/BossThreshold"})
        report.update(expected_original_actor_count=1390, expected_new_actor_count=14,
                      expected_final_actor_count=1404, expected_instance_count=713,
                      new_actor_package_files=sorted(new_actor_files))

    old = {r["path"]: r for r in baseline["backed_up_files"]}
    candidate_names = sorted({MAP_FILE, *files(EA), *files(EO)})
    current = {name: pin(name) for name in candidate_names}
    changed = sorted(p for p in set(old) & set(current) if not same(old[p], current[p]))
    added, removed = sorted(set(current)-set(old)), sorted(set(old)-set(current))
    report.update(baseline_candidate_count=len(old), current_candidate_count=len(current),
                  changed=changed, added=added, removed=removed, current_candidate_files=list(current.values()))
    check("no_candidate_removals", not removed, removed)
    check("new_actor_packages_exact14" if refinement else "new_actor_packages_exact10",
          len(new_actor_files) == (14 if refinement else 10)
          and {p for p in added if p.startswith(EA)} == new_actor_files,
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
    if refinement:
        new_eo_rows = [row for row in eo_rows if row["new"]]
        check("six_new_native_folders_no_other_new_EO", len(new_eo_rows) == len(set(new_folders)) == 6
              and all(set(row["classes"])-METADATA_CLASSES == {"/Script/Engine.ActorFolder"} for row in new_eo_rows),
              [{"path": row["path"], "classes": row["classes"]} for row in new_eo_rows])
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
    check("protected_inventory_only_two_sources_eight_owned_assets" if refinement else "protected_inventory_only_two_sources_seven_owned_assets",
          set(protected_added) == NATIVE_SOURCES | owned_assets,
          {"actual_added": protected_added, "allowed": sorted(NATIVE_SOURCES | owned_assets)})
    report["protected_inventory_added"] = protected_added
    check("owned_namespace_exact_eight_assets" if refinement else "owned_namespace_exact_seven_assets",
          set(files(OWN)) == owned_assets, files(OWN))
    report["owned_assets"] = [pin(p) for p in sorted(owned_assets) if safe(p).is_file()]
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
    parser.add_argument("--profile", choices=("baseline", "refinement"), default="baseline")
    parser.add_argument("--cold-result", type=Path)
    parser.add_argument("--cold-validation", type=Path)
    parser.add_argument("--package-inventory", type=Path)
    parser.add_argument("--source-manifest", type=Path)
    parser.add_argument("--author-plan", type=Path)
    parser.add_argument("--authored-content", type=Path)
    parser.add_argument("--save-result", type=Path)
    parser.add_argument("--material-repair", type=Path)
    parser.add_argument("--boss-plan", type=Path)
    parser.add_argument("--boss-author", type=Path)
    parser.add_argument("--floor-plan", type=Path)
    parser.add_argument("--floor-author", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    try:
        report = build_report(profile=args.profile, cold_result=args.cold_result,
            cold_validation=args.cold_validation, package_inventory=args.package_inventory,
            source_manifest=args.source_manifest, author_plan=args.author_plan,
            authored_content=args.authored_content, save_result=args.save_result,
            material_repair=args.material_repair, boss_plan=args.boss_plan,
            boss_author=args.boss_author, floor_plan=args.floor_plan, floor_author=args.floor_author)
        destination = safe(args.output or OUT / ("final-scope-refinement.json" if args.profile == "refinement" else "final-scope-report.json"))
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
