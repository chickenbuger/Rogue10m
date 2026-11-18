"""Read-only filesystem scope for the completed organic-cave checkpoint.

Pin the1497-file physical checkpoint, permit exactly four existing cave actor
package changes and four new owned assets, and require fresh owned cold/save
evidence. No Unreal import, process control, Git call or asset write occurs.
Only main() writes one fresh JSON report below this feature's evidence folder.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / "Feature/doc/evidence/cave-organic20261118"
OLD = ROOT / "Feature/doc/evidence/caves20261118"
MAP = "Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A"
TARGET = "/Game/"+MAP
MAP_FILE = "Content/"+MAP+".umap"
EA = "Content/__ExternalActors__/"+MAP+"/"
EO = "Content/__ExternalObjects__/"+MAP+"/"
OWN = "Content/Rogue10m/World/Production/WorldLayoutCompactCrop20261114A/NaturalBoundary20261117/"
NEW_OWN = OWN+"Caves20261118/Organic20261118/"
NEW_PACKAGE = "/Game/"+NEW_OWN[8:]
DLL = "Binaries/Win64/UnrealEditor-Rogue10m.dll"
BASELINE_SHA256 = "738b20cd1ad806d56b881c6a61cec88f0c63f8a0f656544d695f73a5ebcf8bbf"
COMMON_SHA256 = "c85d8c6b12ac8913b31d75377ffc7ff567674c030e1d8c1737bcb5d0259d7e11"
PINS = {
    "baseline-live/cave-actors.json": "fff169e2b9fc5ce1993cd0fa8372c038917fb87ee247179761d578a2a8f11ccb",
    "baseline-live/materials.json": "de2e85d4b23ae4ad014d88bcd0bf54d248722ee854e19f808744e5bf71a77afb",
    "baseline-live/result.json": "e81bca59acc743213e58880b1a115c78e6a7338768a14238944c727ae3cfd28d",
    "baseline-live/terrain.json": "a9a87d9ed7aa94c1366d0a9d802a262c598e319c56d188bae64a7c43dfe7be2f",
}
OLD_PINS = {
    "author-live/structures.json": "e0cc6a270fe941b5a29f90433185fddd8e84c3146a7105b23f8aecbad9b1a497",
    "author-live/dressing.json": "f938a675dfb74019eff712d6437946174a4dc58c5cb8aaa25953bda56bdd9dbd",
    "baseline-live/baseline-materials.json": "7d4b2504a5588097cecb803c7637756809a53a2391e09fc4e576f047662984ba",
}
COLD_SCRIPTS = {"Scripts/Editor/OrganicCaves20261118QA.py", "Scripts/Editor/OrganicCaves20261118Cold.py"}
REQUIRED_COLD_CHECKS = {
    "exact1408_actor_identities_preserved_no_new_actors", "all_original1404_actor_and_instance_snapshots_preserved",
    "all_original1404_persistent_components_preserved", "exactly_two_shell_and_two_ramp_visual_components_on_existing_Dressing_actors",
    "four_retired_bins_collision_off_two_floor_bins_hidden_only_other_scene_fields_exact",
    "floor_physics_grassroof_timber_props_lights_other_components_exact",
    "full_global_EV11_5_local_EV3_bounds_and_camera_defaults_preserved",
    "all47_preexisting_material_graphs_parameters_parentage_files_preserved",
    "new_rock_derivative_exact_original43_plus3_recipe_and_floor_material_preserved",
    "all179488_height_samples_bit_exact_and_original_layer_45collision45render_preserved",
    "two_ramp_visuals_reproduce_original_hidden_ramp_exactly_without_extra_collision",
    "all2016_original_capsule_vectors_radii_and_expected_outcomes_pass",
    "dirty_package_identities_unchanged", "saved_clean_after_independent_reload",
} | {site+suffix for site in ("Q", "Z") for suffix in (
    "_one_closed_oriented_manifold_source_shell",
    "_visible_floor_section_upward_horizontal8609_one_cm_below_original_physics",
    "_actual_imported_oriented_triangles_match_source_no_axis_or_degenerate_loss")}


def common_module():
    file = ROOT / "Scripts/CheckNaturalWorldScope20261117.py"
    if hashlib.sha256(file.read_bytes()).hexdigest() != COMMON_SHA256:
        raise RuntimeError("Pinned read-only scope helpers changed")
    spec = importlib.util.spec_from_file_location("organic_scope_readonly_common", file)
    module = importlib.util.module_from_spec(spec)
    old = sys.dont_write_bytecode
    sys.dont_write_bytecode = True
    try:
        spec.loader.exec_module(module)
    finally:
        sys.dont_write_bytecode = old
    return module


def unique_rows(rows, count):
    result = {}
    for row in rows:
        name = row.get("path")
        if not (isinstance(name, str) and name == Path(name).as_posix() and not Path(name).is_absolute()
                and ".." not in Path(name).parts and name not in result
                and isinstance(row.get("size"), int) and not isinstance(row["size"], bool) and row["size"] >= 0
                and re.fullmatch(r"[0-9a-fA-F]{64}", row.get("sha256", ""))):
            raise RuntimeError("Malformed/duplicate immutable manifest row")
        result[name] = row
    if len(rows) != count or len(result) != count:
        raise RuntimeError("Unexpected exact manifest count")
    return result


def manifest_fingerprint(rows):
    canonical = [{key: row[key] for key in ("path", "size", "sha256")} for row in sorted(rows, key=lambda row: row["path"])]
    return hashlib.sha256(json.dumps(canonical, sort_keys=True, separators=(",", ":")).encode()).hexdigest()


def build_report(*, config_path=None, cold_result=None, cold_exit=None, cold_launch=None, save_result=None):
    c = common_module()
    require, safe, read, same = c.require, c.safe, c.read, c.same
    require(ROOT == Path("D:/Project/Rogue10m"), "Exact physical project required")
    cache = {}
    def pin(value):
        file = safe(value)
        key = file.relative_to(ROOT).as_posix()
        if key not in cache:
            cache[key] = c.pin(file)
        return cache[key]
    def compare(expected):
        changed, missing = [], []
        for name, old in expected.items():
            file = safe(name)
            if not file.is_file():
                missing.append(name)
            elif not same(old, pin(file)):
                changed.append({"path": name, "before": old, "after": pin(file)})
        return changed, missing
    baseline_file = BASE / "baseline-files.json"
    require(pin(baseline_file)["sha256"] == BASELINE_SHA256, "Immutable organic-cave disk checkpoint changed")
    baseline = read(baseline_file)
    candidate = unique_rows(baseline["backed_up_files"], 1482)
    owned = unique_rows(baseline["owned_backed_up_files"], 15)
    protected = unique_rows(baseline["protected_files"], 3969)
    saves = unique_rows(baseline["non_selected_save_files"], 458)
    require(baseline["schema"] == "Rogue10m.OrganicCaveDiskCheckpoint.20261118.v1"
            and baseline["status"] == "EXACT1497_PHYSICAL_BACKUP_SHA256_VERIFIED_SCOPED_INPUTS_UNCHANGED"
            and baseline["candidate_map"] == TARGET and baseline["candidate_file_count"] == 1482
            and baseline["owned_file_count"] == 15 and baseline["backed_up_file_count"] == 1497
            and baseline["protected_file_count"] == 3969 and baseline["non_selected_save_file_count"] == 458
            and baseline["no_reparse_paths_verified"] is True and baseline["built_native_sources_match_successful_build"] is True,
            "Exact1482+15/3969/458 physical checkpoint required")
    require(set(owned) <= set(protected) and not set(candidate) & set(owned), "Checkpoint owned/candidate boundary differs")
    require(all(manifest_fingerprint(rows) == baseline["fingerprints"][name] for name, rows in (
        ("candidate", list(candidate.values())), ("owned", list(owned.values())),
        ("protected", list(protected.values())), ("non_selected_saves", list(saves.values())))),
        "Immutable checkpoint category fingerprints differ")
    for relative, expected in PINS.items():
        require(pin(BASE / relative)["sha256"] == expected, "Immutable organic baseline observation changed: "+relative)
    for relative, expected in OLD_PINS.items():
        require(pin(OLD / relative)["sha256"] == expected, "Immutable predecessor receipt changed: "+relative)
    cave_actors = read(BASE / "baseline-live/cave-actors.json")["actors"]
    structures, dressing = read(OLD / "author-live/structures.json"), read(OLD / "author-live/dressing.json")
    actor_rows = [dict(site, label=site.get("label", "CV18_"+site.get("site_id", "")+"_Dressing"))
                  for site in structures["sites"]+dressing["sites"]]
    require(len(actor_rows) == 4 and {row["actor_path"] for row in actor_rows} == set(cave_actors)
            and {row["label"] for row in actor_rows} == {"CV18_"+sid+"_"+role for sid in ("Q", "Z") for role in ("Structure", "Dressing")},
            "Four historical packages must correspond to exactly the actual baseline cave actor paths")
    allowed_actor_packages = {row["package"] for row in actor_rows}
    allowed_actor_files = {c.asset_file(package) for package in allowed_actor_packages}
    require(len(allowed_actor_files) == 4 and allowed_actor_files <= set(candidate)
            and all(name.startswith(EA) for name in allowed_actor_files), "Four existing cave EA packages required")
    config_file = safe(config_path or BASE / "validation-config.json")
    config = read(config_file)
    require(set(config) == {"geometry_spec_sha256", "expected_mesh_paths", "expected_material_path", "expected_floor_material_path"}
            and set(config["expected_mesh_paths"]) == {"Q", "Z"}, "Exact new organic validation configuration required")
    new_material_packages = {NEW_PACKAGE+name for name in ("M_OrganicRock", "MI_OrganicRock")}
    new_mesh_packages = {path.split(".")[0] for path in config["expected_mesh_paths"].values()}
    require(len(new_mesh_packages) == 2 and all(package.startswith(NEW_PACKAGE+"SM_") for package in new_mesh_packages)
            and config["expected_material_path"] == NEW_PACKAGE+"MI_OrganicRock.MI_OrganicRock"
            and config["expected_floor_material_path"] == "/Game/"+OWN[8:]+"Caves20261118/MI_CaveStoneFloor.MI_CaveStoneFloor",
            "Two scoped new meshes and exact original/new material paths required")
    new_packages = new_material_packages | new_mesh_packages
    new_files = {c.asset_file(package) for package in new_packages}
    require(len(new_files) == 4 and not new_files & (set(candidate) | set(owned) | set(protected)),
            "Exactly four new owned asset destinations required")
    geometry_file = BASE / "geometryspec-v2.json"
    geometry = read(geometry_file)
    require(pin(geometry_file)["sha256"] == config["geometry_spec_sha256"]
            and geometry["schema"] == "Rogue10m.OrganicCaveGeometrySpec.20261118.v2", "Exact reviewed V2 geometry required")
    cold_path = safe(cold_result or BASE / "cold-final/result.json")
    cold_dir = cold_path.parent
    cold, validation = read(cold_path), read(cold_dir / "validation.json")
    startup, culture = read(cold_dir / "startup.json"), read(cold_dir / "culture-restoration.json")
    terrain, inventory = read(cold_dir / "terrain-actor-packages.json"), read(cold_dir / "package-inventory.json")
    exit_path = safe(cold_exit or BASE / "cold-host-final/process-exit.json")
    launch_path = safe(cold_launch or exit_path.parent / "launch.json")
    process_exit, launch = read(exit_path), read(launch_path)
    save_path = safe(save_result or BASE / "author-live/final-save-result.json")
    saved = read(save_path)
    require(cold.get("passed") is True and validation.get("passed") is True,
            "Fresh successful organic-cave cold semantic validation required")
    checks, violations = {}, []
    def check(name, condition, detail=None):
        checks[name] = bool(condition)
        if not condition:
            violations.append({"check": name, "detail": detail})
    report = {"schema": "Rogue10m.OrganicCaveFileScope.20261118.v1", "passed": False,
              "utc": datetime.now(timezone.utc).isoformat(), "checks": checks, "violations": violations,
              "asset_writes": False, "unreal_imports": False, "editor_operations": False,
              "process_state_changes": False, "git_operations": False, "baseline_manifest": pin(baseline_file),
              "evidence": {name: pin(file) for name, file in {"config": config_file, "geometry_spec": geometry_file,
                  "cold_result": cold_path, "cold_validation": cold_dir / "validation.json", "cold_startup": cold_dir / "startup.json",
                  "cold_culture": cold_dir / "culture-restoration.json", "cold_terrain": cold_dir / "terrain-actor-packages.json",
                  "cold_package_inventory": cold_dir / "package-inventory.json", "cold_exit": exit_path,
                  "cold_launch": launch_path, "bounded_save": save_path}.items()}}
    check("immutable1497_physical_checkpoint_3969_protected458_saves_and_baseline_observations_pinned", True)
    cold_checks = validation.get("checks", {})
    adapter = validation.get("native_winding_adapter", {})
    check("new_cold_saved_clean_all20_required_semantic_checks", set(cold_checks) == REQUIRED_COLD_CHECKS
          and cold.get("checks") == len(cold_checks) == 20
          and all(value is True for value in cold_checks.values()) and not validation.get("diagnostics")
          and not validation.get("traceback") and validation.get("assets_saved") is False
          and validation.get("dirty_packages_before") == validation.get("dirty_packages_after") == []
          and validation.get("config") == config and adapter.get("triangle_index_mapping") == [0, 2, 1]
          and adapter.get("positions_changed") is False and adapter.get("authored_normals_changed") is False
          and adapter.get("source_artwork_changed") is False,
          {"missing": sorted(REQUIRED_COLD_CHECKS-set(cold_checks)),
              "extra": sorted(set(cold_checks)-REQUIRED_COLD_CHECKS)})
    acceptance = validation.get("capsule_acceptance", {})
    old_queries_file = OLD / "cold-final-validated-live/validation.json"
    require(pin(old_queries_file)["sha256"] == "007323b1908fc1d36350430384a348bfba68b9afd3c77298fdd7ad97b42a1bd6",
            "Original2016 query reference changed")
    original_cases = [row["case"] for row in read(old_queries_file)["capsule_results"]]
    require(len(original_cases) == 2016 and hashlib.sha256(json.dumps(original_cases, sort_keys=True,
        ensure_ascii=False, separators=(",", ":"), allow_nan=False).encode()).hexdigest()
        == "2d0bdc07e7d4eca5a49a5df87d9ad8ff1b3eb3982913fc69d83fa5cf21baecab",
        "Original2016 full case identity changed")
    observed_queries = validation.get("capsule_results", [])
    exact_query_vectors = len(observed_queries) == len(original_cases) and all(
        result.get("start_cm") == case["start"] and result.get("end_cm") == case["end"]
        and result.get("radius_cm") == case["radius"] and result.get("half_height_cm") == 96.0
        and result.get("trace_complex") is False and result.get("channel") == "ECC_Pawn"
        and result.get("blocking_hit") is case["expected_blocking"] and result.get("success") is True
        and result.get("read_only") is True and result.get("dirty_package_names_unchanged") is True
        for case, result in zip(original_cases, observed_queries))
    check("all2016_original_capsule_queries_with_geometry_specific_acceptance_completed", cold.get("capsule_queries") == 2016
          and validation.get("capsule_query_count") == len(validation.get("capsule_results", [])) == 2016
          and acceptance.get("passed") is True and acceptance.get("case_count") == 2016
          and acceptance.get("query_vectors_sha256") == "fde56356bf26d7acad43015455ca09a8faab47c6f7b00598606d90038d3820c4"
          and len(acceptance.get("results", [])) == 2016 and not acceptance.get("failed_cases")
          and [row.get("case_id") for row in acceptance.get("results", [])] == [case["id"] for case in original_cases]
          and all(row.get("passed") is True for row in acceptance.get("results", [])) and exact_query_vectors)
    check("owned_cold_same_PID_semantic_pass_and_observed_exit0", cold.get("pid") == startup.get("pid")
          == process_exit.get("pid") == launch.get("pid") and isinstance(cold.get("pid"), int)
          and process_exit.get("exit_code", process_exit.get("exitCode")) == 0
          and process_exit.get("process_handle_retained") is True and process_exit.get("assets_saved_by_launcher") is False)
    arguments = " ".join(launch.get("arguments", [])).lower()
    check("cold_launch_owned_unattended_english_startup_no_autosave_no_MCP", startup.get("unattended") is True
          and startup.get("nullrhi") is True and startup.get("autosave_and_mcp_autostart_disabled_by_launch") is True
          and startup.get("command_line_stage") == "RogueOrganicCaveStage=cold" and startup.get("assets_saved") is False
          and all(flag in arguments for flag in ("-unattended", "-nullrhi", "-language=en", "-locale=en",
              "-rogueorganiccavestage=cold", "bautosaveenable=false", "bautostartserver=false", "-abslog=",
              "organiccaves20261118cold.py")))
    check("cold_restores_exact_ko_koKR_without_saving_settings", culture.get("before") == {"language": "en", "locale": "en"}
          and culture.get("requested") == culture.get("after") == {"language": "ko", "locale": "ko-KR"}
          and all(culture.get("setter_results", {}).values()) and len(culture.get("setter_results", {})) == 2
          and culture.get("save_to_config") is False and culture.get("assets_saved") is False
          and culture.get("comparison_fields_normalized_or_dropped") is False)
    check("cold_executed_current_new_scripts_and_exact_config", set(startup.get("script_hashes", {})) == COLD_SCRIPTS
          and all(pin(name)["sha256"] == expected for name, expected in startup.get("script_hashes", {}).items())
          and startup.get("validation_config_file_sha256") == pin(config_file)["sha256"])
    terrain_rows = terrain.get("actors", [])
    check("cold_observed17_native_public_terrain_actors_and45_render_collision_unchanged", terrain.get("actor_count")
          == terrain.get("native_python_actor_correspondence_count") == len(terrain_rows) == 17
          and terrain.get("render_components") == terrain.get("collision_components") == 45
          and terrain.get("native_read_only_inspection") is True and terrain.get("native_dirty_package_names_unchanged") is True
          and len({row["package"] for row in terrain_rows}) == 17
          and all(row["landscape_guid"] == "147B73D247543E06FDA1A7AF9D467458" for row in terrain_rows))
    package_rows = inventory.get("packages", [])
    require(isinstance(package_rows, list) and len(package_rows) == len({row["package"] for row in package_rows}),
            "Unique independently observed cold package inventory required")
    packages = {row["package"]: row for row in package_rows}
    check("four_existing_cave_packages_independently_contain_the_same_native_Actor_paths", all(
        row["package"] in packages and "/Script/Engine.Actor" in packages[row["package"]]["classes"]
        and row["actor_path"] in packages[row["package"]]["object_paths"] for row in actor_rows))
    check("four_new_packages_are_only_two_Materials_and_two_StaticMeshes", all(
        package in packages and "/Script/Engine.StaticMesh" in packages[package]["classes"] for package in new_mesh_packages)
        and NEW_PACKAGE+"M_OrganicRock" in packages and "/Script/Engine.Material" in packages[NEW_PACKAGE+"M_OrganicRock"]["classes"]
        and NEW_PACKAGE+"MI_OrganicRock" in packages and "/Script/Engine.MaterialInstanceConstant" in packages[NEW_PACKAGE+"MI_OrganicRock"]["classes"])
    requested = saved.get("requested_packages", [])
    check("bounded_save_covers_exact_four_existing_actors_four_new_assets_no_foreign_dirty", saved.get("success") is True
          and saved.get("native_save_returned") is True and saved.get("save_called") is True
          and saved.get("save_all_dirty_called") is False and saved.get("dirty_packages") == []
          and isinstance(requested, list) and len(requested) == len(set(requested)) == 8
          and set(requested) == set(saved.get("dirty_before", [])) == allowed_actor_packages | new_packages)
    current_candidate = set(c.files(EA.rstrip("/"))) | set(c.files(EO.rstrip("/"))) | {MAP_FILE}
    changed, missing = compare(candidate)
    changed_names = {row["path"] for row in changed}
    added, removed = current_candidate-set(candidate), set(candidate)-current_candidate
    check("candidate_inventory_exact1482_no_additions_or_removals", len(current_candidate) == 1482 and not added and not removed and not missing,
          {"added": sorted(added), "removed": sorted(removed), "missing": missing})
    check("exactly_four_existing_cave_EA_files_changed_no_other_actor_or_Landscape_changes", changed_names == allowed_actor_files,
          {"expected": sorted(allowed_actor_files), "actual": sorted(changed_names)})
    check("map_and_all73_external_objects_byte_preserved", MAP_FILE not in changed_names and not any(name.startswith(EO) for name in changed_names))
    report["candidate"] = {"before_count": 1482, "after_count": len(current_candidate), "changed": changed,
        "added": sorted(added), "removed": sorted(removed), "allowed_actor_packages": sorted(allowed_actor_packages)}
    owned_changed, owned_missing = compare(owned)
    current_owned = set(c.files(OWN.rstrip("/")))
    check("all15_existing_owned_assets_byte_preserved", not owned_changed and not owned_missing,
          {"changed": owned_changed, "missing": owned_missing})
    check("owned_namespace_exact_old15_plus_two_new_Materials_two_new_StaticMeshes", current_owned == set(owned) | new_files,
          {"unexpected": sorted(current_owned-set(owned)-new_files), "missing": sorted((set(owned)|new_files)-current_owned)})
    protected_changed, protected_missing = compare(protected)
    protected_inventory = set()
    directories = [row["path"] for row in baseline["protected_directories"]]
    for directory in directories:
        protected_inventory.update(c.files(directory))
    protected_inventory.update(name for name in protected if not any(name.startswith(directory.rstrip("/")+"/") for directory in directories))
    check("all3969_protected_Source_Config_original_maps_assets_byte_preserved_no_exceptions", not protected_changed and not protected_missing,
          {"changed": protected_changed, "missing": protected_missing})
    check("protected_inventory_adds_exactly_four_declared_owned_assets", protected_inventory == set(protected) | new_files,
          {"unexpected": sorted(protected_inventory-set(protected)-new_files), "missing": sorted((set(protected)|new_files)-protected_inventory)})
    report["protected"] = {"baseline_count": 3969, "current_count": len(protected_inventory),
        "changed": protected_changed, "missing": protected_missing, "new_assets": [pin(name) for name in sorted(new_files)]}
    material_pins = {}
    materials = read(BASE / "baseline-live/materials.json")["materials"]
    for material in materials.values():
        for key in ("disk", "base_disk"):
            row = material[key]
            if "file" in row:
                name = row["file"]
                value = {"path": name, "size": row["size"], "sha256": row["sha256"]}
                require(name not in material_pins or same(material_pins[name], value), "Conflicting immutable material file pins")
                material_pins[name] = value
    material_changed, material_missing = compare(material_pins)
    check("all47_preexisting_materials51_distinct_files_byte_preserved", len(materials) == 47 and len(material_pins) == 51
          and not material_changed and not material_missing, {"changed": material_changed, "missing": material_missing})
    old_materials = read(OLD / "baseline-live/baseline-materials.json")
    mesh_pins = {}
    for key in ("boulder_medium", "boulder_large"):
        row = old_materials["meshes"][key]["disk"]
        mesh_pins[row["file"]] = {"path": row["file"], "size": row["size"], "sha256": row["sha256"]}
    mesh_changed, mesh_missing = compare(mesh_pins)
    check("both_reused_vendor_boulder_meshes_byte_preserved", not mesh_changed and not mesh_missing,
          {"changed": mesh_changed, "missing": mesh_missing})
    source_pins = []
    for site in geometry["sites"]:
        for path_key, hash_key in (("indexed_mesh", "indexed_mesh_sha256"), ("source_obj", "source_obj_sha256")):
            source_pins.append(pin(site[path_key]))
            require(source_pins[-1]["sha256"] == site[hash_key], "Reviewed generated source artwork changed")
    require(pin(geometry["generator"])["sha256"] == geometry["generator_sha256"], "Reviewed V2 generator changed")
    report["source_artwork"] = source_pins
    backup_root = safe(baseline["backup_root"])
    expected_backup = candidate | owned
    expected_backup_names = {safe(backup_root/name).relative_to(ROOT).as_posix() for name in expected_backup}
    backup_names = set(c.files(backup_root))
    backup_changed, backup_missing = [], []
    for name, expected in expected_backup.items():
        file = safe(backup_root/name)
        require(file.is_relative_to(backup_root), "Backup entry escaped exact retained root")
        if not file.is_file():
            backup_missing.append(name)
        elif not same(expected, pin(file)):
            backup_changed.append(name)
    check("all1497_physical_backup_files_exact_inventory_and_original_hashes", len(expected_backup) == 1497
          and backup_names == expected_backup_names and not backup_changed and not backup_missing,
          {"changed": backup_changed, "missing": backup_missing, "unexpected": sorted(backup_names-expected_backup_names)})
    save_changed, save_missing = compare(saves)
    current_saves = {name for name in c.files("Saved/SaveGames") if not c.selected_qa_save(name)}
    check("all458_non_selected_saves_byte_preserved", not save_changed and not save_missing,
          {"changed": save_changed, "missing": save_missing})
    check("non_selected_save_inventory_exact458_only_named_QA_exclusions", current_saves == set(saves),
          {"added": sorted(current_saves-set(saves)), "removed": sorted(set(saves)-current_saves)})
    loaded_dll = startup.get("loaded_project_dll", {})
    check("owned_cold_loaded_current_unchanged_project_DLL", loaded_dll.get("path") == DLL
          and loaded_dll.get("source") == "Current process GetModuleHandleW/GetModuleFileNameW; read-only metadata"
          and pin(DLL)["sha256"] == str(loaded_dll.get("sha256", "")).lower()
          == str(launch.get("dllSha256", "")).lower()
          == "0d2a0c2622e982b93e6d7744a69d09b4d5bce8553a9563430601ada91c901b38")
    report["non_selected_saves"] = {"count": len(saves), "changed": save_changed, "missing": save_missing}
    report.update(passed=all(checks.values()), check_count=len(checks),
        scope="Exact1482 candidate files; four existing cave EA changes only; old15+new4 owned assets;3969 protected inputs;1497 backups;458 non-selected saves.",
        limitations=["No atomic multi-file snapshot or whole-project/all-vendor/generated-cache audit.",
            "Fresh cold scene/collision and file hashes do not certify rendered camera traversal; the276-point normal-game camera walk and six visually reviewed captures remain separate requirements."])
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("config-path", "cold-result", "cold-exit", "cold-launch", "save-result", "output"):
        parser.add_argument("--"+name, type=Path)
    args = parser.parse_args()
    try:
        report = build_report(config_path=args.config_path, cold_result=args.cold_result, cold_exit=args.cold_exit,
            cold_launch=args.cold_launch, save_result=args.save_result)
        c = common_module()
        output = c.safe(args.output or BASE / "final-scope.json")
        c.require(output.is_relative_to(BASE) and output.suffix.lower() == ".json", "Fresh JSON report must stay within organic-cave evidence")
        with output.open("x", encoding="utf-8") as stream:
            json.dump(report, stream, ensure_ascii=False, indent=2, allow_nan=False)
            stream.write("\n")
        print(json.dumps({"passed": report["passed"], "checks": report["checks"], "output": str(output)}, ensure_ascii=False))
        return 0 if report["passed"] else 1
    except (OSError, ValueError, RuntimeError, KeyError, TypeError) as error:
        print(json.dumps({"passed": False, "error": str(error)}, ensure_ascii=False))
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
