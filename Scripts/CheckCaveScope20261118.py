"""Read-only final cave file audit after saved-clean independent cold QA.

No Unreal import, editor/process control, Git call, asset write, save or repair.
The original1473 candidate files, eight owned assets,3962 protected inputs and
458 non-selected saves remain the comparison boundary. Only a fresh final JSON
report is written; old failures and reports are never replaced.
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
BASE = ROOT / "Feature/doc/evidence/caves20261118"
NATURAL = ROOT / "Feature/doc/evidence/landscape-natural-boundary20261117"
BASELINE_SHA256 = "19b2df14473db127464c5aceb74e59d1f399a35fb1cab0e190377400bb0a8dfd"
BASELINE_ARTIFACT_SHA256 = {
    "baseline-live/baseline-materials.json": "7d4b2504a5588097cecb803c7637756809a53a2391e09fc4e576f047662984ba",
    "baseline-live/baseline-actors.json": "f5390322017e3a501811a105f72686c11e90a8c0ed662484aee43404569eacf5",
    "baseline-live/baseline-native.json": "4da5cf558a5a7b4471bffc8cb04ce43399d6b41edb10149378f80b0724792ec9",
}
PLAN_SHA256 = "39b0150690e02a6edb453f91d0362603d9063dc3771dce9977d870fe184e6a74"
DRESSING_SHA256 = "4177a9e1b8544e16cd61217cf64fd01a5c47da8e6ef3ee84dea25228107449db"
VISIBILITY_SHA256 = "5509ec2f171704b4b963cb82726c9660f4e2c5b05ac883394ef63e72de644d81"
WALL_V1_SHA256 = "a4127f8ca450c50c114001d60c92e05da98c380c9597c22715f382c3827b3cd8"
TRIPLANAR_SHA256 = "0d24c669a60f9c998401afcc7f7c75e74f6fe1f72c499496fda3139e5c47353f"
EXPOSURE_SHA256 = "bc58e611db30ae46e604be7b3c72a3af02a94a57a6eef288faf23eb5feea8627"
BOUNDS_SHA256 = "fb2cb4bd5e9108507dee21d16af499c1ef19679ebbe029244705b2d55586fc70"
BOUNDS_RECEIPT_SHA256 = "f8f002c576404a05c9686ef83d16b027c939f5422b81bdd67837c55d27ced349"
CAMERA_RELIEF_SHA256 = "92e2464e246528952718203c7e82ed7455c20102fa7b8c5586835229f02fb92f"
CAMERA_RELIEF_RECEIPT_SHA256 = "e718d352f102417e29cacb6d3f8cec2964ee4bae7f9e60b2e6b2b9c13595623a"
CAPSULE_CASES_SHA256 = "2d0bdc07e7d4eca5a49a5df87d9ad8ff1b3eb3982913fc69d83fa5cf21baecab"
ORIGINAL_LAYER = "7b30bb0271250f6f4b2d4164c72940c83a3e49b13078ef91e9316992816a0955"
TARGET_BLAKE3 = "b9f28e82d7661e83555d5f72942ea999ca6aea9554969753a889ea50f87e1970"
TARGET_UINT16_SHA256 = "ef9d5119c138e680001442fac76499e2e0ff40e9f51e428a33bd834aa3b4ffc1"
COLLAR_BLAKE3 = "67d6682a3bad4b9b480d0b4e264903e9665846b562d7b72fa5cd8ff10e0252fc"
COLLAR_UINT16_SHA256 = "a2c8846963f7d08f7df561b6c800f2f389255e6f52c14a1453a6e5e3f7674726"
COLLAR_FILE_SHA256 = "cd7928198b0f9c3cfc0d2ad5944a49cca4debc3d1b3ca69b63c946b35873752c"
COLLAR_SPEC_SHA256 = "4f3d5dacfdee8af8e00e32c6db3597208061e2593e488f74cdd88de334a3696a"
COLLAR_PREFLIGHT_SHA256 = "2fd5f9dd3c8d57eb723d215ac724724b54234e8b23389971fe4e91f67a8e84ca"
LANDSCAPE_GUID = "147B73D247543E06FDA1A7AF9D467458"
MAP = "Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A"
TARGET = "/Game/" + MAP
EA = "Content/__ExternalActors__/" + MAP + "/"
EO = "Content/__ExternalObjects__/" + MAP + "/"
MAP_FILE = "Content/" + MAP + ".umap"
OWN = "Content/Rogue10m/World/Production/WorldLayoutCompactCrop20261114A/NaturalBoundary20261117/"
CAVES = OWN + "Caves20261118/"
NEW_MATERIALS = {CAVES + name + ".uasset" for name in (
    "M_CaveSurface", "MI_CaveGrass", "MI_CaveStoneFloor", "MI_CaveRock",
    "M_CaveLamp", "MI_CaveLampWarm", "MI_CaveLampCool")}
TERRAIN_CLASSES = {"/Script/Landscape.Landscape", "/Script/Landscape.LandscapeStreamingProxy"}
METADATA_CLASSES = {"/Script/CoreUObject.Package", "/Script/CoreUObject.MetaData"}
DLL = "Binaries/Win64/UnrealEditor-Rogue10m.dll"
COLD_SCRIPTS = {"Scripts/Editor/ValidateCaves20261118.py", "Scripts/Editor/ColdCaves20261118.py"}
REQUIRED_COLD_CHECKS = {
    "registered45_collision45_two_layers_settled", "original_raw_paint_layer_and_landscape_guid_preserved",
    "all179488_composite_samples_equal_reviewed_target", "only_two_declared_local_excavation_masks_changed",
    "all29_road19m_envelopes_and_three_flat_villages_preserved", "ridge_and_nonflat_source_samples_unchanged",
    "outside_declared_cut_envelopes_equal_source", "exact1408_actors_four_declared_additions",
    "all_original1404_actor_snapshots_preserved", "all_original_persistent_components_preserved",
    "new_cave_actors_always_loaded_visible_no_tick",
    "two_bounded_EV3_query_shapes_ignore_all_channels_with_public_distance_proof",
    "full_global_EV11_5_and_camera_defaults_preserved",
    "all286_structure_instances_transform_and_PawnCameraVisibility_collision",
    "all426_dressing_instances_bounds_rotation_and_camera_only_collision", "all12_lights_exact_visible_settings",
    "all40_source_material_graphs_parameters_files_unchanged", "all_eight_preexisting_owned_asset_files_unchanged",
    "all_seven_new_material_graphs_parameters_parentage_match_authored_state",
    "surface_triplanar43_nodes_original13_and_visibility_controls_preserved",
    "exact_two_wall_ceiling_binding_overlays_follow_frozen_recipe_chain",
    "surface_preserves_original_ground_graph_and_exact_latest_triplanar_state",
    "exact150cm_collar_target_and_preserved_v1_predecessor",
    "exact_two_Q_camera_relief_X50_matrices_and_other424_dressing_preserved",
    "all_unique_route_edges_both_directions_two_radii_three_lanes_clear", "all112_floor_and10_ramp_supports_exact",
    "all112_ceilings_and10_surface_roofs_exact", "all20_wall_crossings_hit_exact_structure",
    "all_planned_capsule_queries_completed_without_failures", "dirty_package_identities_unchanged",
    "saved_clean_after_independent_reload",
}


def common_module():
    spec = importlib.util.spec_from_file_location("cave_scope_readonly_common", ROOT / "Scripts/CheckNaturalWorldScope20261117.py")
    value = importlib.util.module_from_spec(spec)
    old = sys.dont_write_bytecode
    sys.dont_write_bytecode = True
    try:
        spec.loader.exec_module(value)
    finally:
        sys.dont_write_bytecode = old
    return value


def unique_rows(rows, expected_count):
    result = {}
    for row in rows:
        relative = row.get("path")
        if not (isinstance(relative, str) and relative == Path(relative).as_posix()
                and not Path(relative).is_absolute() and ".." not in Path(relative).parts
                and relative not in result and isinstance(row.get("size"), int) and not isinstance(row["size"], bool)
                and row["size"] >= 0 and re.fullmatch(r"[0-9a-fA-F]{64}", row.get("sha256", ""))):
            raise RuntimeError("Malformed/duplicate immutable file manifest row")
        result[relative] = row
    if len(rows) != expected_count or len(result) != expected_count:
        raise RuntimeError("Unexpected immutable manifest file count")
    return result


def build_report(*, cold_result=None, cold_validation=None, terrain_packages=None,
                 package_inventory=None, cold_startup=None, cold_exit=None,
                 final_save=None, checkpoint_save=None, pre_collar_save=None, exposure_save=None,
                 camera_relief_save=None):
    c = common_module()
    require, safe, read, same = c.require, c.safe, c.read, c.same
    require(ROOT == Path("D:/Project/Rogue10m"), "Exact physical project required")
    pins = {}

    def pin(value):
        absolute = safe(value)
        key = absolute.relative_to(ROOT).as_posix()
        if key not in pins:
            pins[key] = c.pin(absolute)
        return pins[key]

    def compare(expected):
        changed, missing = [], []
        for relative, old in expected.items():
            file = safe(relative)
            if not file.is_file():
                missing.append(relative)
            elif not same(old, pin(file)):
                changed.append({"path": relative, "before": old, "after": pin(file)})
        return changed, missing

    baseline_file = BASE / "baseline-files.json"
    require(pin(baseline_file)["sha256"] == BASELINE_SHA256, "Immutable pre-cave baseline artifact changed")
    baseline = read(baseline_file)
    candidate = unique_rows(baseline["backed_up_files"], 1473)
    old_owned = unique_rows(baseline["owned_backed_up_files"], 8)
    protected = unique_rows(baseline["protected_files"], 3962)
    saves = unique_rows(baseline["non_selected_save_files"], 458)
    require(baseline["schema"] == "Rogue10m.CaveDiskCheckpoint.20261118.v1"
            and baseline["status"] == "EXACT1481_PHYSICAL_BACKUP_SHA256_VERIFIED_SCOPED_INPUTS_UNCHANGED"
            and baseline["candidate_map"] == TARGET and baseline["candidate_file_count"] == 1473
            and baseline["owned_file_count"] == 8 and baseline["backed_up_file_count"] == 1481
            and baseline["protected_file_count"] == 3962 and baseline["non_selected_save_file_count"] == 458
            and baseline["no_reparse_paths_verified"] is True
            and baseline["built_native_sources_match_successful_build"] is True,
            "Exact1473+8/3962/458 physical checkpoint required")
    require(set(old_owned) == c.REFINEMENT_ASSETS and set(old_owned) <= set(protected)
            and not set(candidate) & set(old_owned), "Exact eight old OWN assets required")
    baseline_artifacts = {relative: pin(BASE / relative) for relative in BASELINE_ARTIFACT_SHA256}
    require(all(row["sha256"] == BASELINE_ARTIFACT_SHA256[relative] for relative, row in baseline_artifacts.items()),
            "Immutable original material/actor/native observation artifact changed")

    cold_path = safe(cold_result or BASE / "cold-live/result.json")
    validation_path = safe(cold_validation or cold_path.parent / "validation.json")
    inventory_path = safe(package_inventory or cold_path.parent / "package-inventory.json")
    terrain_path = safe(terrain_packages or cold_path.parent / "terrain-actor-packages.json")
    startup_path = safe(cold_startup or cold_path.parent / "startup.json")
    culture_path = safe(cold_path.parent / "culture-restoration.json")
    exit_path = safe(cold_exit or BASE / "cold-host/process-exit.json")
    final_save_path = safe(final_save or BASE / "author-live/final-save-result.json")
    checkpoint_save_path = safe(checkpoint_save or BASE / "author-live/structure-checkpoint-save-result.json")
    pre_collar_save_path = safe(pre_collar_save or BASE / "author-live/collar-pre-save-result.json")
    exposure_save_path = safe(exposure_save or BASE / "author-live/exposure-save-result.json")
    camera_relief_save_path = safe(camera_relief_save or BASE / "author-live/camera-relief-save-result.json")
    cold, validation, startup, process_exit = read(cold_path), read(validation_path), read(startup_path), read(exit_path)
    culture = read(culture_path)
    require(cold.get("passed") is True and validation.get("passed") is True,
            "Successful new-feature independent cold result and validation are required before file audit")
    structures = read(BASE / "author-live/structures.json")
    dressing = read(BASE / "author-live/dressing.json")
    materials = read(BASE / "author-live/materials.json")
    visibility = read(BASE / "author-live/visibility.json")
    triplanar = read(BASE / "author-live/triplanar.json")
    exposure = read(BASE / "author-live/exposure.json")
    exposure_spec = read(BASE / "exposure-spec.json")
    bounds = read(BASE / "author-live/exposure-bounds.json")
    bounds_spec = read(BASE / "exposure-bounds-spec.json")
    relief = read(BASE / "author-live/camera-relief.json")
    relief_spec = read(BASE / "camera-relief-spec.json")
    v1_applied = read(BASE / "author-live/apply-height-confirmed.json")
    applied = read(BASE / "author-live/apply-height-collar.json")
    collar_spec, collar_preflight = read(BASE / "collar-spec.json"), read(BASE / "collar-preflight.json")
    v1_target, collar_target = read(BASE / "target-height-plan.json"), read(BASE / "target-height-plan-collar.json")
    plan = read(BASE / "plan.json")
    final_saved, checkpoint_saved = read(final_save_path), read(checkpoint_save_path)
    pre_collar_saved = read(pre_collar_save_path)
    exposure_saved = read(exposure_save_path)
    camera_relief_saved = read(camera_relief_save_path)
    baseline_native = read(BASE / "baseline-live/baseline-native.json")
    baseline_actors = read(BASE / "baseline-live/baseline-actors.json")
    checks, violations = {}, []

    def check(name, value, detail=None):
        checks[name] = bool(value)
        if not value:
            violations.append({"check": name, "detail": detail})

    report = {"schema": "Rogue10m.CaveFileScope.20261118.v1", "passed": False,
              "utc": datetime.now(timezone.utc).isoformat(), "checks": checks, "violations": violations,
              "baseline_manifest": pin(baseline_file), "baseline_observations": baseline_artifacts,
              "asset_writes": False, "unreal_imports": False,
              "editor_operations": False, "process_state_changes": False, "git_operations": False}
    report["evidence"] = {key: pin(file) for key, file in {
        "cold_result": cold_path, "cold_validation": validation_path, "cold_startup": startup_path,
        "cold_culture_restoration": culture_path,
        "observed_cold_exit": exit_path, "terrain_packages": terrain_path, "package_inventory": inventory_path,
        "final_save": final_save_path, "checkpoint_save": checkpoint_save_path, "pre_collar_save": pre_collar_save_path,
        "exposure_save": exposure_save_path,
        "camera_relief_save": camera_relief_save_path,
        "height_revision_v1": BASE / "author-live/apply-height-confirmed.json",
        "height_revision_final_collar": BASE / "author-live/apply-height-collar.json",
        "collar_target": BASE / "target-height-plan-collar.json", "collar_spec": BASE / "collar-spec.json",
        "collar_preflight": BASE / "collar-preflight.json",
        "structures": BASE / "author-live/structures.json", "dressing": BASE / "author-live/dressing.json",
        "materials": BASE / "author-live/materials.json", "visibility": BASE / "author-live/visibility.json",
        "triplanar": BASE / "author-live/triplanar.json", "exposure": BASE / "author-live/exposure.json",
        "exposure_spec": BASE / "exposure-spec.json", "exposure_bounds": BASE / "author-live/exposure-bounds.json",
        "exposure_bounds_spec": BASE / "exposure-bounds-spec.json", "camera_relief": BASE / "author-live/camera-relief.json",
        "camera_relief_spec": BASE / "camera-relief-spec.json"}.items()}
    check("immutable_checkpoint_exact1473_candidate8_owned3962_protected458_saves", True)
    check("original_material_actor_native_observation_artifacts_sha256_pinned", True)
    check("reviewed_plans_target_grid_and_original_layer_receipts", pin(BASE / "plan.json")["sha256"] == PLAN_SHA256
          and pin(BASE / "dressing-plan.json")["sha256"] == DRESSING_SHA256
          and pin(BASE / "visibility-spec.json")["sha256"] == VISIBILITY_SHA256
          and pin(BASE / "wall-texture-spec.json")["sha256"] == WALL_V1_SHA256
          and pin(BASE / "triplanar-spec.json")["sha256"] == TRIPLANAR_SHA256
          and pin(BASE / "target-height-plan.json")["sha256"] == plan["target_height_plan_sha256"]
          and plan["preflight_summary"]["target_uint16le_sha256"] == TARGET_UINT16_SHA256
          and v1_applied.get("success") is True and v1_applied.get("original_layer_preserved") is True
          and v1_applied.get("layers_created") == 0 and v1_applied.get("existing_layer_revised") is True
          and v1_applied.get("previous_composite_blake3") == baseline_native["composite_blake3"]
          and v1_applied.get("target_composite_blake3") == TARGET_BLAKE3
          and v1_applied.get("expected_original_layer_blake3") == ORIGINAL_LAYER == baseline_native["original_layer_blake3"])
    check("latest150cm_collar_exact_pinned_grid_chain_not_arbitrary_fresh_hash",
          pin(BASE / "collar-spec.json")["sha256"] == COLLAR_SPEC_SHA256
          and pin(BASE / "collar-preflight.json")["sha256"] == COLLAR_PREFLIGHT_SHA256
          and pin(BASE / "target-height-plan-collar.json")["sha256"] == COLLAR_FILE_SHA256
          and collar_spec["schema"] == "Rogue10m.CaveFloorCollarSpec.20261118.v1"
          and collar_spec["source_v1_native_blake3"] == TARGET_BLAKE3
          and collar_spec["source_v1_grid_uint16le_sha256"] == TARGET_UINT16_SHA256
          and pin(BASE / "author-live/apply-height-confirmed.json")["sha256"]
              == collar_spec["source_v1_apply_receipt_sha256"] == "c4d5b5024ff1389c2bc8879730f546f639111445c9bd3304163176a7f0b0c93f"
          and collar_spec["target_uint16le_sha256"] == COLLAR_UINT16_SHA256
          and collar_spec["floor_collar_cm"] == collar_spec["remaining_blend_cm"] == 150
          and collar_spec["old_outer_blend_envelope_cm"] == 300
          and collar_spec["probe_cases_and_expectations_changed"] is False
          and collar_spec["geometry_actor_material_camera_changes"] == 0
          and collar_preflight["passed"] is True and collar_preflight["recipe_sha256"] == COLLAR_SPEC_SHA256
          and collar_preflight["target_height_plan_file_sha256"] == COLLAR_FILE_SHA256
          and collar_preflight["target_uint16le_sha256"] == COLLAR_UINT16_SHA256
          and all(collar_preflight[key] is True for key in ("all_29_road_sample_heights_preserved",
              "all_three_village_circles_and_grid_collar_preserved", "all_new_changes_inside_old_pit300cm_envelope",
              "all_new_changes_lower_only", "all_original_pit_interior_and_ramp_heights_unchanged",
              "all_wall_midface_quads_flat8600", "outside_changed_index_set_exactly_preserved"))
          and collar_preflight["target_changed_from_v1_samples"] == sum(a != b for a, b in
              zip(v1_target["target_heights"], collar_target["target_heights"])) == 447
          and applied.get("success") is True and applied.get("original_layer_preserved") is True
          and applied.get("existing_layer_revised") is True and applied.get("layers_created") == 0
          and applied.get("previous_composite_blake3") == TARGET_BLAKE3
          and applied.get("target_composite_blake3") == COLLAR_BLAKE3
          and applied.get("expected_original_layer_blake3") == ORIGINAL_LAYER
          and applied.get("new_layer_guid") == collar_spec["new_layer_guid"]
          and applied.get("changed_samples") == 447)
    cold_checks = validation.get("checks", {})
    check("independent_cold_saved_clean_all_semantic_checks", validation.get("schema") == "Rogue10m.CaveValidation.20261118.v1"
          and validation.get("require_saved_clean") is True and not validation.get("diagnostics")
          and validation.get("traceback") is None and validation.get("assets_saved") is False
          and validation.get("actor_or_property_mutations") is False
          and validation.get("dirty_packages_before") == validation.get("dirty_packages_after") == []
          and REQUIRED_COLD_CHECKS <= set(cold_checks) and all(value is True for value in cold_checks.values())
          and cold.get("assets_saved") is False and cold.get("checks") == len(cold_checks)
          and cold.get("capsule_queries") == validation.get("capsule_query_count") == 2016)
    case_rows = [row["case"] for row in validation.get("capsule_results", [])]
    check("all2016_original_capsule_case_bytes_unchanged_after_camera_repair", len(case_rows) == 2016
          and hashlib.sha256(json.dumps(case_rows, sort_keys=True, ensure_ascii=False, separators=(",", ":")).encode("utf-8")).hexdigest()
              == CAPSULE_CASES_SHA256)
    check("cold_full179488_target_and_original1404_semantics", validation.get("composite_uint16le_sha256") == COLLAR_UINT16_SHA256
          and len(collar_target["target_heights"]) == 179488
          and validation.get("changed_height_samples") == collar_preflight["changed_samples"] == sum(a != b for a, b in
              zip(baseline_native["composite_heights_uint16"], collar_target["target_heights"])) == 3300
          and all(cold_checks.get(name) is True for name in (
              "all179488_composite_samples_equal_reviewed_target", "original_raw_paint_layer_and_landscape_guid_preserved",
              "all_original1404_actor_snapshots_preserved", "all_original_persistent_components_preserved")))
    check("visibility_refinement_bounded_new_components_only_global_camera_preserved", visibility.get("success") is True
          and visibility.get("schema") == "Rogue10m.CaveVisibilityRefinement.20261118.v1"
          and visibility.get("spec_sha256") == VISIBILITY_SHA256 and visibility.get("actors_created") == []
          and len(set(visibility.get("created_component_paths", []))) == 4
          and visibility.get("original1404_baseline_preserved") is True
          and visibility.get("all1408_existing_geometry_snapshots_equal") is True
          and visibility.get("camera_properties_mutated") is False and visibility.get("terrain_mutated") is False
          and visibility.get("global_postprocess_before") == visibility.get("global_postprocess_after")
          and visibility.get("camera_cdo_postprocess_before") == visibility.get("camera_cdo_postprocess_after"))
    check("final_triplanar43_shader_and_two_material_bindings_follow_recipe", triplanar.get("success") is True
          and triplanar.get("schema") == "Rogue10m.CaveTriplanarStoneRefinement.20261118.v1"
          and triplanar.get("spec_sha256") == TRIPLANAR_SHA256
          and triplanar.get("created_assets") == triplanar.get("created_actors") == triplanar.get("created_components") == []
          and triplanar.get("original13_nodes_preserved") is True
          and triplanar.get("existing_visibility_B_and_Alpha_preserved") is True
          and triplanar.get("source40_materials_preserved") is True and triplanar.get("original1404_preserved") is True
          and triplanar.get("all286_transforms_and_all_geometry_collision_preserved") is True
          and triplanar.get("all_lighting_camera_exposure_preserved") is True
          and len(triplanar.get("surface_graph_after", {}).get("nodes", {})) == 43)
    exposure_sites = exposure.get("sites", [])
    before_effects, after_effects = exposure.get("effects_before", {}), exposure.get("effects_after", {})
    before_local, after_local = before_effects.get("local_pp", {}), after_effects.get("local_pp", {})
    local_paths = {site["post_process_path"] for site in exposure_spec["sites"]}
    exact_exposure_delta = (set(before_local) == set(after_local) == local_paths and len(local_paths) == 2
        and all(before_effects.get(key) == after_effects.get(key) for key in ("global_pp", "camera", "lights")))
    for local_path in local_paths:
        old, new = before_local.get(local_path, {}), after_local.get(local_path, {})
        expected_text, substitutions = re.subn(r"(?<=[,(])(AutoExposureMinBrightness|AutoExposureMaxBrightness)=3\.000000(?=[,)])",
            lambda match: match[1] + "=0.000000", old.get("settings", ""))
        exact_exposure_delta = (exact_exposure_delta and substitutions == 2 and new.get("settings") == expected_text
            and {k: v for k, v in old.items() if k != "settings"} == {k: v for k, v in new.items() if k != "settings"})
    check("unsaved_EV0_predecessor_exact_recipe_and_no_other_effect_state_change",
          pin(BASE / "exposure-spec.json")["sha256"] == EXPOSURE_SHA256
          and exposure_spec.get("schema") == "Rogue10m.CaveExposureSpec.20261118.v1"
          and exposure_spec.get("map") == TARGET and exposure_spec.get("before_ev100") == 3
          and exposure_spec.get("after_ev100") == 0 and exposure_spec.get("nominal_exposure_multiplier") == 8
          and exposure_spec.get("native_composite_blake3") == COLLAR_BLAKE3
          and exposure_spec.get("original_layer_blake3") == ORIGINAL_LAYER
          and pin(BASE / "author-live/visibility.json")["sha256"] == exposure_spec["visibility_receipt_sha256"]
          and pin(BASE / "author-live/triplanar.json")["sha256"] == exposure_spec["triplanar_receipt_sha256"]
          and exposure.get("schema") == "Rogue10m.CaveExposureTune.20261118.v1"
          and exposure.get("success") is True and exposure.get("spec_sha256") == EXPOSURE_SHA256
          and exposure.get("created_assets") == exposure.get("created_actors") == exposure.get("created_components") == []
          and exposure.get("assets_saved") is False and exposure.get("save_called") is False
          and exposure.get("original1404_preserved") is True
          and exposure.get("all_geometry_collision_lights_camera_global_exposure_preserved") is True
          and exposure.get("source40_and_new7_material_states_preserved") is True
          and exposure.get("no_overrides_added") is True and exposure.get("only_two_local_min_max_values_changed") is True
          and exposure.get("dirty_before") == [] and len(exposure.get("dirty_after", [])) == 2
          and set(exposure.get("dirty_after", [])) == set(dressing.get("packages", []))
          and len(exposure_sites) == 2 and {site.get("post_process_path") for site in exposure_sites} == local_paths
          and all(site.get("only_min_max_changed") is True and site.get("before_ev100") == 3
                  and site.get("after_ev100") == 0 for site in exposure_sites)
          and exact_exposure_delta)
    bounds_sites = bounds.get("sites", [])
    bounds_before, bounds_after = bounds.get("effects_before", {}), bounds.get("effects_after", {})
    old_local, new_local = bounds_before.get("local_pp", {}), bounds_after.get("local_pp", {})
    restored_effects_exact = (bounds_before == exposure.get("effects_after")
        and set(old_local) == set(new_local) == local_paths
        and all(bounds_before.get(key) == bounds_after.get(key) for key in ("global_pp", "camera", "lights")))
    for local_path in local_paths:
        old, new = old_local.get(local_path, {}), new_local.get(local_path, {})
        expected_text, substitutions = re.subn(r"(?<=[,(])(AutoExposureMinBrightness|AutoExposureMaxBrightness)=0\.000000(?=[,)])",
            lambda match: match[1] + "=3.000000", old.get("settings", ""))
        restored_effects_exact = (restored_effects_exact and substitutions == 2
            and new.get("settings") == expected_text == before_local[local_path]["settings"]
            and {k: v for k, v in old.items() if k != "settings"} == {k: v for k, v in new.items() if k != "settings"})
    check("final_QueryOnly_ignore_all_bounds_and_restored_EV3_exact_recipe_chain",
          pin(BASE / "exposure-bounds-spec.json")["sha256"] == BOUNDS_SHA256
          and pin(BASE / "author-live/exposure-bounds.json")["sha256"] == BOUNDS_RECEIPT_SHA256
          and bounds_spec.get("schema") == "Rogue10m.CaveExposureBoundsSpec.20261118.v1"
          and bounds_spec.get("map") == TARGET and bounds_spec.get("native_composite_blake3") == COLLAR_BLAKE3
          and bounds_spec.get("original_layer_blake3") == ORIGINAL_LAYER
          and bounds_spec.get("supersedes_no_collision_fallback_hypothesis") is True
          and bounds_spec.get("ev0_candidate_visually_accepted") is False
          and pin(BASE / "author-live/exposure.json")["sha256"] == bounds_spec["exposure_receipt_sha256"]
          and pin(BASE / "author-live/visibility.json")["sha256"] == bounds_spec["visibility_receipt_sha256"]
          and pin(BASE / "author-live/triplanar.json")["sha256"] == bounds_spec["triplanar_receipt_sha256"]
          and bounds.get("schema") == "Rogue10m.CaveExposureBoundsRepair.20261118.v1" and bounds.get("success") is True
          and bounds.get("spec_sha256") == BOUNDS_SHA256 and bounds.get("exposure_receipt_sha256") == bounds_spec["exposure_receipt_sha256"]
          and bounds.get("created_assets") == bounds.get("created_actors") == bounds.get("created_components") == []
          and bounds.get("assets_saved") is False and bounds.get("save_called") is False
          and bounds.get("original1404_preserved") is True
          and bounds.get("all_geometry_lights_camera_global_exposure_materials_preserved") is True
          and bounds.get("only_two_box_query_modes_and_local_min_max_changed") is True
          and bounds.get("public_inside_outside_distance_proof_passed") is True
          and bounds.get("no_pawn_camera_visibility_blocking_added") is True
          and set(bounds.get("dirty_before", [])) == set(bounds.get("dirty_after", [])) == set(dressing.get("packages", []))
          and len(bounds_sites) == 2 and {site.get("post_process_path") for site in bounds_sites} == local_paths
          and all(site.get("before_ev100") == 0 and site.get("after_ev100") == 3
                  and site.get("before_collision") == "NO_COLLISION" and site.get("after_collision") == "QUERY_ONLY"
                  and site.get("all_collision_responses") == "IGNORE" and site.get("all_channels_ignore") is True
                  and site.get("box_collision_after") == "<CollisionEnabled.QUERY_ONLY: 1>"
                  and site.get("box_profile_after") == "Custom" and site.get("new_overrides") == 0
                  and site.get("overlap_events") is False and site.get("navigation") is False and site.get("tick_enabled") is False
                  and abs(site["after_center_query"]["distance_cm"]) <= .01
                  and abs(site["after_outside_query"]["distance_cm"] - 500) <= 1 for site in bounds_sites)
          and restored_effects_exact)
    q_dressing = next(site for site in dressing["sites"] if site["site_id"] == "Q")
    relief_changes = relief.get("instance_changes", [])
    expected_relief = relief_spec.get("instance_changes", [])
    relief_delta_exact = (len(relief_changes) == len(expected_relief) == 2
        and {change.get("index") for change in relief_changes} == {120, 121}
        and {change.get("label") for change in relief_changes} == {"Q_Rock_Q_Wall_V_03_060", "Q_Rock_Q_Wall_V_03_061"})
    for change, expected in zip(relief_changes, expected_relief):
        old, new = change.get("before_world_transform", {}), change.get("after_world_transform", {})
        relief_delta_exact = (relief_delta_exact and {key: change.get(key) for key in expected} == expected
            and change.get("actor_path") == q_dressing["actor_path"] and change.get("package") == q_dressing["package"]
            and change.get("component_path") == q_dressing["actor_path"] + ".Q_CaveDressingISM_00"
            and change.get("world_translation_delta_cm") == [50, 0, 0]
            and old.get("p", [])[1:] == new.get("p", [])[1:] and new["p"][0] - old["p"][0] == 50
            and old.get("q") == new.get("q") and old.get("s") == new.get("s")
            and change.get("observed_world_transform_before") == old and change.get("observed_world_transform_after") == new)
    check("final_two_diagnosed_Q_rock_X50_repair_exact_receipt_chain_no_gate_relaxation",
          pin(BASE / "camera-relief-spec.json")["sha256"] == CAMERA_RELIEF_SHA256
          and pin(BASE / "author-live/camera-relief.json")["sha256"] == CAMERA_RELIEF_RECEIPT_SHA256
          and relief_spec.get("schema") == "Rogue10m.CaveCameraReliefSpec.20261118.v1" and relief_spec.get("map") == TARGET
          and relief_spec.get("all_2016_capsule_cases_unchanged") is True and relief_spec.get("all_276_runtime_waypoints_unchanged") is True
          and relief_spec.get("camera_minimum_distance_cm_unchanged") == 140
          and relief_spec.get("camera_sustained_max_pull_in_cm_unchanged") == 180
          and relief_spec.get("collision_and_materials_unchanged") is True and relief_spec.get("new_actors_components_instances_assets") == 0
          and relief.get("schema") == "Rogue10m.CaveCameraRelief.20261118.v1" and relief.get("success") is True
          and relief.get("spec_sha256") == CAMERA_RELIEF_SHA256 and relief.get("input_sha256") == relief_spec.get("input_sha256")
          and all(pin(BASE / relative)["sha256"] == digest for relative, digest in relief_spec["input_sha256"].items())
          and relief.get("created_assets") == relief.get("created_actors") == relief.get("created_components") == []
          and relief.get("assets_saved") is False and relief.get("save_called") is False
          and relief.get("original1404_preserved") is True and relief.get("all_other_instances_and_components_preserved") is True
          and relief.get("checked_instance_matrix_predecessors") == {"structure": 286, "dressing": 426}
          and relief.get("changed_instance_count") == 2 and relief.get("thresholds_and_routes_unchanged") is True
          and relief.get("actual_camera_pass_claimed") is False and relief.get("composite_blake3") == COLLAR_BLAKE3
          and relief.get("original_layer_blake3") == ORIGINAL_LAYER and relief.get("dirty_before") == []
          and relief.get("dirty_after") == [q_dressing["package"]]
          and relief.get("actor_snapshot_sha256_before") == relief.get("normalized_actor_snapshot_sha256_after")
          and relief.get("effects_before") == relief.get("effects_after") == bounds.get("effects_after")
          and relief_delta_exact)
    check("observed_independent_cold_exit0_exact_pid", process_exit.get("exitCode") == 0
          and isinstance(cold.get("pid"), int) and cold["pid"] == startup.get("pid") == process_exit.get("pid")
          and cold["pid"] != read(BASE / "baseline-live/result.json")["pid"])
    check("cold_launch_owned_unattended_no_autosave_no_MCP", startup.get("target") == TARGET
          and startup.get("unattended") is True and startup.get("nullrhi") is True
          and startup.get("autosave_and_mcp_autostart_disabled_by_launch") is True
          and startup.get("assets_saved") is False and startup.get("command_line_stage") == "RogueCaveStage=cold")
    check("cold_owned_process_restores_exact_baseline_Korean_culture_without_config_or_field_changes",
          startup.get("startup_culture") == culture.get("before") == {"language": "en", "locale": "en"}
          and startup.get("validation_culture_requested") == culture.get("requested") == culture.get("after")
              == {"language": "ko", "locale": "ko-KR"}
          and startup.get("culture_restore_save_to_config") is False
          and culture.get("schema") == "Rogue10m.CaveColdCultureRestoration.20261118.v1"
          and culture.get("setter_results") == {"language": True, "locale": True}
          and culture.get("save_to_config") is False and culture.get("assets_saved") is False
          and culture.get("comparison_fields_normalized_or_dropped") is False
          and cold.get("validation_language") == "ko" and cold.get("validation_locale") == "ko-KR"
          and cold.get("culture_saved_to_config") is False)
    script_hashes = startup.get("script_hashes", {})
    check("cold_validator_and_launcher_hashes_match_current_sources", set(script_hashes) == COLD_SCRIPTS
          and all(pin(relative)["sha256"].lower() == digest.lower() for relative, digest in script_hashes.items()))

    new_ea_packages = set(structures.get("packages", [])) | set(dressing.get("packages", []))
    new_ea = {c.asset_file(package) for package in new_ea_packages}
    new_eo_packages = {name for name in structures.get("dirty_after", [])
                       if name.startswith("/Game/__ExternalObjects__/" + MAP + "/")}
    new_eo_packages |= set(dressing.get("new_folder_external_packages", []))
    new_eo = {c.asset_file(package) for package in new_eo_packages}
    new_owned = {c.asset_file(package) for package in materials.get("created_assets", [])}
    terrain_package_set = set(applied.get("dirty_packages", []))
    terrain_files = {c.asset_file(package) for package in terrain_package_set}
    check("successful_bounded_author_receipts_exact_four_actors_seven_assets", structures.get("success") is True
          and dressing.get("success") is True and materials.get("success") is True
          and structures.get("existing1404_actor_snapshots_equal") is True
          and dressing.get("original1404_baseline_preserved") is True
          and dressing.get("all1406_existing_actor_and_component_snapshots_equal") is True
          and structures.get("instance_count") == 286 and dressing.get("instance_count") == 426 and dressing.get("light_count") == 12
          and len(new_ea) == len(new_ea_packages) == 4 and all(p.startswith(EA) for p in new_ea)
          and len(new_eo) == len(new_eo_packages) == 5 and all(p.startswith(EO) for p in new_eo)
          and new_owned == NEW_MATERIALS and not (new_ea | new_eo) & set(candidate))
    check("collar_native_dirty_allowlist_same17_original_terrain_as_v1", len(terrain_package_set) == 17
          and terrain_package_set == set(v1_applied["dirty_packages"]))
    allowed_save_packages = terrain_package_set | new_ea_packages | new_eo_packages | set(materials["created_assets"])

    def save_ok(receipt):
        names, before = receipt.get("requested_packages", []), receipt.get("dirty_before", [])
        return (receipt.get("success") is True and receipt.get("native_save_returned") is True
                and receipt.get("save_called") is True and receipt.get("save_all_dirty_called") is False
                and receipt.get("map") == TARGET and receipt.get("dirty_packages") == []
                and isinstance(names, list) and names and len(names) == len(set(names))
                and isinstance(before, list) and len(before) == len(set(before))
                and set(before) == set(names) and set(names) <= allowed_save_packages)

    check("final_native_bounded_save_success_no_foreign_dirty", save_ok(final_saved)
          and set(final_saved.get("requested_packages", [])) == terrain_package_set
          and len(final_saved.get("requested_packages", [])) == 17)
    check("initial_structure_checkpoint_save_success_no_foreign_dirty", save_ok(checkpoint_saved))
    check("pre_collar_WIP_save_success_no_foreign_dirty", save_ok(pre_collar_saved)
          and len(pre_collar_saved.get("requested_packages", [])) == 24)
    check("latest_exposure_bounded_save_exact_two_dressing_packages_no_foreign_dirty", save_ok(exposure_saved)
          and len(exposure_saved.get("requested_packages", [])) == 2
          and set(exposure_saved.get("requested_packages", [])) == set(dressing.get("packages", [])))
    check("latest_camera_relief_bounded_save_exact_one_Q_dressing_package_no_foreign_dirty", save_ok(camera_relief_saved)
          and camera_relief_saved.get("requested_packages") == [q_dressing["package"]])
    saved_union = (set(final_saved.get("requested_packages", [])) | set(checkpoint_saved.get("requested_packages", []))
                   | set(pre_collar_saved.get("requested_packages", [])) | set(exposure_saved.get("requested_packages", []))
                   | set(camera_relief_saved.get("requested_packages", [])))
    check("five_save_receipts_cover_exact33_allowed_packages", len(allowed_save_packages) == 33 and saved_union == allowed_save_packages,
          {"expected": sorted(allowed_save_packages), "saved": sorted(saved_union)})

    terrain = read(terrain_path)
    terrain_rows = terrain.get("actors", [])
    classes = [row.get("class") for row in terrain_rows]
    class_map_valid = (terrain.get("schema") == "Rogue10m.CaveColdTerrainActors.20261118.v1"
        and terrain.get("actor_count") == len(terrain_rows) == len(terrain_package_set) == 17
        and {row.get("package") for row in terrain_rows} == terrain_package_set
        and classes.count("/Script/Landscape.Landscape") == 1 and classes.count("/Script/Landscape.LandscapeStreamingProxy") == 16
        and terrain.get("render_components") == terrain.get("collision_components") == 45
        and sum(row.get("render_components", -100) for row in terrain_rows) == 45
        and sum(row.get("collision_components", -100) for row in terrain_rows) == 45
        and len({row.get("actor_guid") for row in terrain_rows}) == 17
        and all(row.get("landscape_guid") == LANDSCAPE_GUID and row.get("class") in TERRAIN_CLASSES
                and re.fullmatch(r"[0-9a-fA-F]{32}", row.get("actor_guid", ""))
                and row["actor_guid"] != "0" * 32 and row.get("actor_path") in baseline_actors["actors"]
                and baseline_actors["actors"][row["actor_path"]]["class"] == row["class"]
                and baseline_actors["packages"][row["actor_path"]] == row["package"] for row in terrain_rows))
    check("all17_existing_changed_allowlist_packages_are_observed_terrain_only", class_map_valid, terrain_rows)
    require(class_map_valid, "Independent cold classification must bind the exact17 native-height packages to original Landscape/proxy actors")

    current_candidate_names = set(c.files(EA.rstrip("/"))) | set(c.files(EO.rstrip("/"))) | {MAP_FILE}
    removed = sorted(set(candidate) - current_candidate_names)
    added = sorted(current_candidate_names - set(candidate))
    changed, missing = compare(candidate)
    changed_names = {row["path"] for row in changed}
    check("final_candidate_exact1482_files", len(current_candidate_names) == 1482)
    check("no_candidate_removals", not removed and not missing, {"removed": removed, "missing": missing})
    check("new_external_actors_exact_four_declared", {p for p in added if p.startswith(EA)} == new_ea)
    check("new_external_objects_exact_five_declared_folders", {p for p in added if p.startswith(EO)} == new_eo)
    check("no_other_candidate_additions", set(added) == new_ea | new_eo, added)
    check("existing_candidate_changes_only_native17_terrain_packages", changed_names <= terrain_files
          and all(p.startswith(EA) for p in terrain_files) and terrain_files <= set(candidate), sorted(changed_names - terrain_files))
    check("map_and_all_original_external_objects_byte_preserved", MAP_FILE not in changed_names
          and not any(p.startswith(EO) for p in changed_names) and not removed)
    inventory = read(inventory_path)
    package_rows = inventory.get("packages", [])
    require(isinstance(package_rows, list) and len(package_rows) == len({row["package"] for row in package_rows}),
            "Unique independent cold package-class inventory required")
    package_map = {row["package"]: row for row in package_rows}
    check("all_five_new_external_objects_are_only_native_actor_folders", all(
        package in package_map and set(package_map[package]["classes"]) - METADATA_CLASSES == {"/Script/Engine.ActorFolder"}
        for package in new_eo_packages))
    check("four_new_external_actor_packages_contain_native_actors", all(
        package in package_map and "/Script/Engine.Actor" in package_map[package]["classes"] for package in new_ea_packages))
    check("cold_package_metadata_contains_all17_terrain_classes", all(row["package"] in package_map
          and row["class"] in package_map[row["package"]]["classes"] for row in terrain_rows))
    report["candidate"] = {"before_count": 1473, "after_count": len(current_candidate_names),
        "changed_count": len(changed), "added_count": len(added), "removed_count": len(removed),
        "changed": changed, "added": [pin(p) for p in added], "removed": removed,
        "terrain_allowed_packages": sorted(terrain_package_set), "new_actor_packages": sorted(new_ea_packages),
        "new_folder_packages": sorted(new_eo_packages)}
    report["save_receipt_roles"] = {"initial": "Structure/material checkpoint only",
        "pre_collar": "WIP clean prerequisite for native revision; not QA acceptance",
        "final": "Latest collar terrain saved; require all independent cold semantic checks",
        "exposure": "Bounded two-Dressing bounds/EV3 repair; EV0 predecessor was not saved",
        "camera_relief": "Final bounded one-Q-Dressing two-rock X50 repair; require fresh independent cold semantic checks and actual-camera visual review separately"}

    owned_changed, owned_missing = compare(old_owned)
    current_owned_names = set(c.files(OWN.rstrip("/")))
    check("all_eight_old_owned_assets_byte_preserved", not owned_changed and not owned_missing,
          {"changed": owned_changed, "missing": owned_missing})
    check("owned_namespace_exact_old8_plus_new7", current_owned_names == set(old_owned) | NEW_MATERIALS
          and current_owned_names - set(old_owned) == new_owned, sorted(current_owned_names))
    protected_changed, protected_missing = compare(protected)
    protected_inventory = set()
    directories = [row["path"] for row in baseline["protected_directories"]]
    for directory in directories:
        protected_inventory.update(c.files(directory))
    protected_inventory.update(relative for relative in protected if not any(
        relative.startswith(directory.rstrip("/") + "/") for directory in directories))
    check("all3962_protected_inputs_byte_preserved_no_source_or_build_exceptions", not protected_changed and not protected_missing,
          {"changed": protected_changed, "missing": protected_missing})
    check("protected_inventory_only_seven_declared_owned_additions", protected_inventory == set(protected) | NEW_MATERIALS,
          {"added": sorted(protected_inventory - set(protected)), "removed": sorted(set(protected) - protected_inventory)})
    report["protected"] = {"baseline_count": 3962, "current_count": len(protected_inventory),
                           "byte_unchanged_count": len(protected) - len(protected_changed) - len(protected_missing),
                           "new_allowed": [pin(p) for p in sorted(NEW_MATERIALS)]}

    material_baseline = read(BASE / "baseline-live/baseline-materials.json")
    material_pins = {}
    for material in material_baseline["materials"].values():
        for key in ("disk", "base_disk"):
            row = material[key]
            if "file" in row:
                relative = row["file"]
                value = {"path": relative, "size": row["size"], "sha256": row["sha256"]}
                require(relative not in material_pins or same(material_pins[relative], value), "Conflicting source-material file hashes")
                material_pins[relative] = value
    source_changed, source_missing = compare(material_pins)
    check("all40_source_materials_and44_distinct_base_files_unchanged", len(material_baseline["materials"]) == 40
          and len(material_pins) == 44 and not source_changed and not source_missing,
          {"changed": source_changed, "missing": source_missing})
    mesh_pins = {}
    for key in ("boulder_medium", "boulder_large"):
        row = material_baseline["meshes"][key]["disk"]
        mesh_pins[row["file"]] = {"path": row["file"], "size": row["size"], "sha256": row["sha256"]}
    mesh_changed, mesh_missing = compare(mesh_pins)
    check("two_reused_vendor_boulder_mesh_files_unchanged", len(mesh_pins) == 2 and not mesh_changed and not mesh_missing,
          {"changed": mesh_changed, "missing": mesh_missing})
    report["source_material_files"] = [pin(p) for p in sorted(material_pins)]
    report["source_mesh_files"] = [pin(p) for p in sorted(mesh_pins)]

    backup_root = safe(baseline["backup_root"])
    expected_backup = dict(candidate)
    expected_backup.update(old_owned)
    backup_names = {safe(backup_root / p).relative_to(ROOT).as_posix() for p in expected_backup}
    actual_backup_names = set(c.files(backup_root))
    backup_changed, backup_missing = [], []
    for relative, expected in expected_backup.items():
        file = safe(backup_root / relative)
        require(file.is_relative_to(backup_root), "Checkpoint entry escaped the exact backup root")
        if not file.is_file():
            backup_missing.append(relative)
        elif not same(expected, pin(file)):
            backup_changed.append(relative)
    check("physical_backup_exact1481_files_and_every_original_hash", len(expected_backup) == 1481
          and actual_backup_names == backup_names and not backup_changed and not backup_missing,
          {"changed": backup_changed, "missing": backup_missing, "unexpected": sorted(actual_backup_names - backup_names)})

    source_manifest_path = NATURAL / "build/source-hashes-refinement.json"
    source_manifest = read(source_manifest_path)
    build = read(source_manifest_path.parent / source_manifest["build_result"])
    required_native = {"Source/Rogue10m/Editor/Rogue10mNaturalBoundary20261117Authoring.h",
                       "Source/Rogue10m/Editor/Rogue10mNaturalBoundary20261117Authoring.cpp", "Source/Rogue10m/Rogue10m.Build.cs"}
    source_rows = source_manifest.get("files", [])
    check("compiled_three_native_sources_still_match_successful_build", build.get("exitCode") == 0
          and len(source_rows) == 3 and {row["path"] for row in source_rows} == required_native
          and all(pin(row["path"])["sha256"].lower() == row["sha256"].lower() for row in source_rows))
    live_launch = read(NATURAL / "refinement-host/launch.json")
    loaded_dll = startup.get("loaded_project_dll", {})
    check("cold_loaded_dll_hash_matches_live_launch_and_current_binary", loaded_dll.get("path") == DLL
          and loaded_dll.get("source") == "Current process GetModuleHandleW/GetModuleFileNameW; read-only metadata"
          and pin(DLL)["sha256"].lower() == str(loaded_dll.get("sha256", "")).lower()
          == live_launch["dllSha256"].lower())
    report["native_build"] = {"source_manifest": pin(source_manifest_path), "build_result": pin(source_manifest_path.parent / source_manifest["build_result"]),
                              "current_dll": pin(DLL), "cold_loaded_dll": loaded_dll}

    save_changed, save_missing = compare(saves)
    current_saves = {relative for relative in c.files("Saved/SaveGames") if not c.selected_qa_save(relative)}
    check("all458_non_selected_saves_byte_preserved", not save_changed and not save_missing,
          {"changed": save_changed, "missing": save_missing})
    check("non_selected_save_inventory_exact458_with_only_named_QA_exclusions", current_saves == set(saves),
          {"added": sorted(current_saves - set(saves)), "removed": sorted(set(saves) - current_saves)})
    report["non_selected_saves"] = {"count": len(saves), "changed": save_changed, "missing": save_missing,
        "allowed_QA_names_only": [relative for relative in c.files("Saved/SaveGames") if c.selected_qa_save(relative)]}
    report.update(passed=all(checks.values()), check_count=len(checks),
        scope="Exact candidate map/EA/EO; old8+new7 OWN; immutable3962 inputs;44source-material files/two Boulder meshes;1481 physical backups;458non-selected saves.",
        limitations=["No whole-project/all-vendor/generated-cache audit or atomic multi-file filesystem snapshot.",
                     "File hashes/class metadata complement independent cold structure/collision QA; this report does not certify rendered camera traversal, full combat, physical input or Shipping.",
                     "Only original Landscape/proxy package changes are permitted; no Build.cs or source exceptions."])
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("cold-result", "cold-validation", "terrain-packages", "package-inventory", "cold-startup", "cold-exit",
                 "final-save", "checkpoint-save", "pre-collar-save", "exposure-save", "camera-relief-save", "output"):
        parser.add_argument("--" + name, type=Path)
    args = parser.parse_args()
    try:
        report = build_report(cold_result=args.cold_result, cold_validation=args.cold_validation,
            terrain_packages=args.terrain_packages, package_inventory=args.package_inventory, cold_startup=args.cold_startup,
            cold_exit=args.cold_exit, final_save=args.final_save, checkpoint_save=args.checkpoint_save,
            pre_collar_save=args.pre_collar_save, exposure_save=args.exposure_save, camera_relief_save=args.camera_relief_save)
        c = common_module()
        output = c.safe(args.output or BASE / "final-scope.json")
        c.require(output.is_relative_to(BASE) and output.suffix.lower() == ".json",
                  "Fresh JSON scope report must remain in this feature's evidence directory")
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
