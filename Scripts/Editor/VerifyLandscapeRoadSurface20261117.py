"""Read-only validation after an owner-confirmed save and independent reload.

Import is inert. The owner loads the exact candidate and its complete editor
region, then calls validate(reload_confirmed=True). No level/region loading,
save, compile, material editing, actor editing, PIE or process control occurs.
This can run in the owner's prepared Editor-Cmd process after authoring/save.
CPU data checks do not replace prior rendered visual or CharacterMovement QA.
"""
from __future__ import annotations

from collections import Counter
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path
import traceback


ROOT = Path("D:/Project/Rogue10m")
TARGET = "/Game/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A"
OWN = "/Game/Rogue10m/World/Production/WorldLayoutCompactCrop20261114A/RoadSurface20261117"
MATERIAL = OWN + "/M_RoadSurface"
SOURCE_MATERIAL = "/Game/Rogue10m/World/StartVillage/Materials/MI_StartVillage_EarthPath"
BASE_TAG = "Rogue10m.WorldLayout20261112"
SURFACE_TAG = "Rogue10m.LandscapeRoadSurface20261117"
PREFIX = "WL26_RoadSurface20261117_"
FOLDER_PACKAGE = "/Game/__ExternalObjects__/Rogue10m/Maps/Production/WorldLayoutCompactCrop20261114A/4/U0/7LSR0EPX24RNQB6FXJ4SNF"
RESUME = ROOT / "Feature/doc/evidence/landscape-resume-20261117"
BASELINE = RESUME / "workbench/cold-reload"
ROAD = ROOT / "Feature/doc/evidence/landscape-road-surface20261117"
OUT = ROAD / "final-cold"
PROFILE_SLOT = "Rogue10m_Town_QA_Landscape20261116A"
KEY_SLOT = "Rogue10m_Fighter_QA_Landscape20261116A"
PROGRESSION_PREFIX = "Rogue10m_Progression_QA_Landscape20261116A"
READ_ONLY_KEYS = ("texture_sources_unchanged", "layer_metadata_unchanged",
    "landscape_selection_and_editing_layer_unchanged", "editor_pointer_unchanged",
    "loaded_uobject_identities_and_flags_unchanged", "editor_object_selection_unchanged",
    "transaction_queue_and_active_state_unchanged", "transaction_handle_readable",
    "dirty_package_names_unchanged")
TERRAIN_KEYS = ("metadata_digest", "semantic_snapshot_digest", "composite", "raw_edit_layers",
    "edit_layers", "paint_layers", "landscape_guid", "landscape_actor_path", "material_path",
    "actor_origin_cm", "actor_scale", "actor_rotation_pitch_yaw_roll", "layout", "components")


def require(value, message):
    if not value:
        raise RuntimeError(message)


def read(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def digest(path):
    value = hashlib.sha256()
    with path.open("rb") as stream:
        while chunk := stream.read(1024 * 1024):
            value.update(chunk)
    return value.hexdigest().upper()


def write(name, value):
    with (OUT / (name + ".json")).open("x", encoding="utf-8") as stream:
        json.dump(value, stream, ensure_ascii=False, indent=2)
        stream.write("\n")


def asset(value):
    return value.get_path_name().split(".")[0] if value else None


def xyz(value):
    return [float(value.x), float(value.y), float(value.z)]


def close(a, b, tolerance):
    return len(a) == len(b) and all(math.isfinite(x) and math.isfinite(y)
                                   and abs(x - y) <= tolerance for x, y in zip(a, b))


def rotation_basis(pitch, yaw, roll):
    # UE FRotationMatrix unit axes; compare orientations through the -90deg
    # Euler singularity without accepting a different projector orientation.
    sp, cp = math.sin(math.radians(pitch)), math.cos(math.radians(pitch))
    sy, cy = math.sin(math.radians(yaw)), math.cos(math.radians(yaw))
    sr, cr = math.sin(math.radians(roll)), math.cos(math.radians(roll))
    return [cp*cy, cp*sy, sp, sr*sp*cy-cr*sy, sr*sp*sy+cr*cy, -sr*cp,
            -(cr*sp*cy+sr*sy), cy*sr-cr*sp*sy, cr*cp]


def inventory(actor):
    rotation = actor.get_actor_rotation()
    return {"path": actor.get_path_name(), "class": actor.get_class().get_path_name(),
            "label": actor.get_actor_label(), "location": xyz(actor.get_actor_location()),
            "rotation": [rotation.pitch, rotation.yaw, rotation.roll],
            "scale": xyz(actor.get_actor_scale3d()),
            "tags": [str(tag) for tag in actor.tags]}


def protected_file_check(rows, directory):
    changed, missing = [], []
    for row in rows:
        relative = row.get("path", row.get("name"))
        path = directory / relative
        if not path.is_file():
            missing.append(relative)
        elif path.stat().st_size != row["size"] or digest(path) != row["sha256"].upper():
            changed.append(relative)
    return {"checked": len(rows), "changed": changed, "missing": missing,
            "passed": not changed and not missing}


def permitted_qa_save(name):
    return name in (PROFILE_SLOT + ".sav", KEY_SLOT + ".sav") or (
        name.startswith(PROGRESSION_PREFIX + "_") and name.endswith(".sav"))


def validate(*, reload_confirmed=False):
    require(reload_confirmed is True, "Owner must save and independently reload the exact candidate first")
    require(not OUT.exists(), "Preserve existing final-cold evidence; do not rerun over it")
    import unreal as u

    editor = u.get_editor_subsystem(u.UnrealEditorSubsystem)
    levels = u.get_editor_subsystem(u.LevelEditorSubsystem)
    world = editor.get_editor_world()

    def dirty():
        return sorted({p.get_path_name() for p in
            list(u.EditorLoadingAndSavingUtils.get_dirty_map_packages())
            + list(u.EditorLoadingAndSavingUtils.get_dirty_content_packages())})

    def guard():
        require(Path(u.Paths.project_dir()).resolve() == ROOT, "Wrong exact project")
        require(world is not None and editor.get_editor_world() == world and asset(world) == TARGET
                and not levels.is_in_play_in_editor() and not editor.get_game_world(),
                "Exact editor candidate with PIE stopped required; nothing is loaded by this script")
        require(not dirty(), "Independent saved/clean checkpoint required")

    guard()
    plan = read(ROAD / "preflight.json")
    baseline = read(BASELINE / "retained-height-paint-layers.json")
    baseline_dependencies = read(BASELINE / "retained-dependencies.json")
    baseline_actors = read(BASELINE / "actors.json")
    require(plan.get("status") == "read_only_preflight_passed_prototype_not_authored"
            and plan.get("target") == TARGET and len(plan.get("decals", [])) == 29
            and len(plan.get("source_roads", [])) == 663, "Exact 29-road/663-source plan required")
    require(baseline.get("success") is True and baseline_dependencies.get("success") is True
            and baseline_dependencies.get("loaded_reflection_walk_complete") is True,
            "Strict workbench baseline is incomplete")
    OUT.mkdir(parents=True, exist_ok=False)
    report = {"status": "started", "utc": datetime.now(timezone.utc).isoformat(),
              "process_id": os.getpid(), "target": TARGET, "owner_confirmed_saved_reload": True,
              "baseline": BASELINE.relative_to(ROOT).as_posix(), "checks": {},
              "boundary": "Saved CPU Landscape data, complete loaded reflected-property walk, actor whitelist and scoped file/save hashes only. No native/custom serializer, unloaded/transitive reference, new visual or gameplay certification."}
    write("context", report)
    try:
        checks = report["checks"]
        actor_system = u.get_editor_subsystem(u.EditorActorSubsystem)
        actors = list(actor_system.get_all_level_actors())
        actual_rows = [inventory(actor) for actor in actors]
        by_path = {row["path"]: row for row in actual_rows}
        checks["unique_actor_paths"] = len(by_path) == len(actors)
        base_paths = {row["path"] for row in baseline_actors}
        sources = {row["actor_path"]: row for row in plan["source_roads"]}
        source_labels = {row["label"] for row in plan["source_roads"]}
        checks["unique_sources663"] = len(sources) == len(source_labels) == 663
        checks["all_original_actor_paths_preserved"] = base_paths <= set(by_path)
        checks["original_actor_inventory_fields_preserved"] = all(by_path.get(row["path"]) == row for row in baseline_actors)
        source_failures = []
        source_packages = {}
        for actor in actors:
            before = sources.get(actor.get_path_name())
            if before is None:
                continue
            component = actor.get_component_by_class(u.StaticMeshComponent)
            source_package = actor.get_package().get_name()
            require(source_package.startswith("/Game/__ExternalActors__/" + TARGET[6:] + "/"),
                    "Foreign source-road package: " + actor.get_actor_label())
            require((ROOT / "Content" / (source_package[6:] + ".uasset")).is_file(),
                    "Source-road package is not saved: " + actor.get_actor_label())
            source_packages[actor.get_actor_label()] = source_package
            rotation = actor.get_actor_rotation()
            actual = {"label": actor.get_actor_label(), "actor_path": actor.get_path_name(),
                      "component_path": component.get_path_name() if component else None,
                      "tags": sorted(map(str, actor.get_editor_property("tags"))),
                      "location_cm": xyz(actor.get_actor_location()), "scale": xyz(actor.get_actor_scale3d()),
                      "rotation_pitch_yaw_roll": [rotation.pitch, rotation.yaw, rotation.roll],
                      "component_visible": bool(component.get_editor_property("visible")) if component else None,
                      "actor_hidden_in_game": bool(actor.get_editor_property("hidden")),
                      "collision_profile": str(component.get_collision_profile_name()) if component else None,
                      "materials": [asset(component.get_material(i)) for i in range(component.get_num_materials())] if component else []}
            expected = dict(before, component_visible=False, actor_hidden_in_game=True)
            if (actual != expected or not isinstance(actor, u.StaticMeshActor) or not component
                    or asset(component.get_editor_property("static_mesh")) != "/Engine/BasicShapes/Cube"
                    or actual["collision_profile"] != "NoCollision"):
                source_failures.append(actual["label"])
        checks["source663_only_visibility_changes"] = not source_failures and set(sources) <= set(by_path)
        checks["source663_unique_saved_native_packages"] = (len(source_packages) == 663
            and len(set(source_packages.values())) == 663)
        report["source_failures"] = source_failures
        report["source_packages"] = dict(sorted(source_packages.items()))

        new_actors = [actor for actor in actors if actor.get_path_name() not in base_paths]
        expected_decals = {row["label"]: row for row in plan["decals"]}
        observed_labels, projector_rows = set(), []
        for actor in new_actors:
            label = actor.get_actor_label()
            expected = expected_decals.get(label)
            require(expected is not None and label not in observed_labels and isinstance(actor, u.DecalActor),
                    "Unexpected new/duplicate/nondecal actor: " + label)
            observed_labels.add(label)
            component = actor.get_component_by_class(u.DecalComponent)
            require(component is not None, "Missing native DecalComponent")
            # UE5.8 ADecalActor creates two editor-only visualization primitives
            # (DecalActor.cpp:35-36). They are not gameplay collision geometry.
            # Permit only those exact native subobjects, with collision disabled.
            primitive_rows = []
            for primitive in actor.get_components_by_class(u.PrimitiveComponent):
                primitive_rows.append({"name": primitive.get_name(),
                    "class": primitive.get_class().get_path_name(),
                    "path": primitive.get_path_name(),
                    "editor_only": bool(primitive.get_editor_property("bIsEditorOnly")),
                    "collision_profile": str(primitive.get_collision_profile_name()),
                    "collision_enabled": str(primitive.get_collision_enabled())})
            primitive_rows.sort(key=lambda row: row["name"])
            native_visualizers_only = (len(primitive_rows) == 2
                and {(row["name"], row["class"]) for row in primitive_rows} == {
                    ("ArrowComponent0", "/Script/Engine.ArrowComponent"),
                    ("Sprite", "/Script/Engine.BillboardComponent")}
                and all(row["editor_only"] is True and row["collision_profile"] == "NoCollision"
                    and row["collision_enabled"] == str(u.CollisionEnabled.NO_COLLISION)
                    for row in primitive_rows))
            rotation = actor.get_actor_rotation()
            wanted = expected["rotation"]
            facts = {"label": label, "path": actor.get_path_name(),
                     "package": actor.get_package().get_name(),
                     "location_cm": xyz(actor.get_actor_location()),
                     "rotation": [rotation.pitch, rotation.yaw, rotation.roll],
                     "scale": xyz(actor.get_actor_scale3d()),
                     "local_half_extents_cm": xyz(component.get_editor_property("decal_size")),
                     "material": asset(component.get_decal_material()),
                     "visible": bool(component.get_editor_property("visible")),
                     "hidden": bool(actor.get_editor_property("hidden")),
                     "folder": str(actor.get_folder_path()),
                     "fade_screen_size": float(component.get_editor_property("fade_screen_size")),
                     "primitive_components": len(primitive_rows),
                     "primitive_readback": primitive_rows,
                     "native_editor_visualizers_only_no_gameplay_collision": native_visualizers_only}
            require(set(map(str, actor.get_editor_property("tags"))) == {BASE_TAG, SURFACE_TAG}
                    and close(facts["location_cm"], expected["center_cm"], 0.01)
                    and close(facts["scale"], [1, 1, 1], 0.0001)
                    and close(rotation_basis(rotation.pitch, rotation.yaw, rotation.roll),
                              rotation_basis(wanted["pitch"], wanted["yaw"], wanted["roll"]), 0.0001)
                    and close(facts["local_half_extents_cm"], expected["local_half_extents_cm"], 0.01)
                    and facts["material"] == MATERIAL and facts["visible"] and not facts["hidden"]
                    and facts["folder"] == "RoadSurface20261117" and facts["fade_screen_size"] == 0.0
                    and native_visualizers_only, "Projector readback mismatch: " + label)
            package = facts["package"]
            require(package.startswith("/Game/__ExternalActors__/" + TARGET[6:] + "/"), "Foreign projector package")
            require((ROOT / "Content" / (package[6:] + ".uasset")).is_file(), "Projector package is not saved")
            projector_rows.append(facts)
        checks["new_actors_exactly29_projectors"] = len(new_actors) == 29 and observed_labels == set(expected_decals)
        report["projectors"] = projector_rows

        # Read only the saved registry tags, using the public AssetData API.
        # UActorFolder's FolderLabel is private and is never read directly.
        registry = u.AssetRegistryHelpers.get_asset_registry()
        folders = list(registry.get_assets_by_package_name(u.Name(FOLDER_PACKAGE), True, False))
        require(len(folders) == 1, "Expected exactly one saved native road folder asset")
        folder = folders[0]
        folder_class = folder.asset_class_path
        folder_facts = {"package": str(folder.package_name),
                        "class": str(folder_class.package_name) + "." + str(folder_class.asset_name),
                        "tags": {name: folder.get_tag_value(name) for name in
                                 ("FolderLabel", "FolderIsDeleted", "OuterPackageName")},
                        "query_only_on_disk": True}
        report["new_road_folder"] = folder_facts
        checks["one_exact_saved_native_road_folder"] = (folder_facts["package"] == FOLDER_PACKAGE
            and folder_facts["class"] == "/Script/Engine.ActorFolder"
            and folder_facts["tags"] == {"FolderLabel": "RoadSurface20261117", "FolderIsDeleted": "0", "OuterPackageName": TARGET})
        require(checks["one_exact_saved_native_road_folder"], "Saved native road folder tags/class mismatch")

        completed_disk = read(RESUME / "road-save-completed-files.json")
        source_files = {"Content/" + package[6:] + ".uasset" for package in source_packages.values()}
        projector_files = {"Content/" + row["package"][6:] + ".uasset" for row in projector_rows}
        folder_file = "Content/" + FOLDER_PACKAGE[6:] + ".uasset"
        checks["completed_disk_diff_exact_native_package_whitelist"] = (
            completed_disk.get("status") == "completed_native_save_disk_scope_requires_cold"
            and completed_disk.get("before_count") == 1423 and completed_disk.get("after_count") == 1453
            and len(completed_disk.get("changed", [])) == 663 and set(completed_disk["changed"]) == source_files
            and len(completed_disk.get("added", [])) == 30 and set(completed_disk["added"]) == projector_files | {folder_file}
            and completed_disk.get("removed") == [] and len(source_files) == 663 and len(projector_files) == 29)
        report["completed_candidate_file_hashes"] = protected_file_check(completed_disk["current_files"], ROOT)
        checks["completed_candidate1453_file_hashes_unchanged"] = (
            len(completed_disk["current_files"]) == 1453 and report["completed_candidate_file_hashes"]["passed"])

        material = u.load_asset(MATERIAL)
        require(isinstance(material, u.Material)
                and material.get_editor_property("material_domain") == u.MaterialDomain.MD_DEFERRED_DECAL
                and material.get_editor_property("blend_mode") == u.BlendMode.BLEND_TRANSLUCENT,
                "Saved owned road material domain/blend mismatch")
        require((ROOT / "Content" / (MATERIAL[6:] + ".uasset")).is_file(), "Owned material is not saved")
        source_material = u.load_asset(SOURCE_MATERIAL)
        require(isinstance(source_material, u.MaterialInstanceConstant), "Source EarthPath missing")
        lib = u.MaterialEditingLibrary
        tint = lib.get_material_instance_vector_parameter_value(source_material, "ColorTint")
        checks["source_appearance_unchanged"] = (asset(lib.get_material_instance_texture_parameter_value(source_material, "BaseColorTexture")) == plan["texture"]
            and close([tint.r, tint.g, tint.b, tint.a], plan["tint_rgba"], 0.000001))
        expressions = list(lib.get_material_expressions(material))
        class_counts = dict(Counter(node.get_class().get_path_name() for node in expressions))
        expected_classes = {"/Script/Engine.MaterialExpressionWorldPosition": 1,
            "/Script/Engine.MaterialExpressionComponentMask": 1,
            "/Script/Engine.MaterialExpressionScalarParameter": 3,
            "/Script/Engine.MaterialExpressionMultiply": 2,
            "/Script/Engine.MaterialExpressionTextureSample": 1,
            "/Script/Engine.MaterialExpressionVectorParameter": 1}
        require(len(expressions) == 9 and class_counts == expected_classes, "Owned material must contain exactly the planned nine native nodes")
        textures = [node for node in expressions if isinstance(node, u.MaterialExpressionTextureSample)]
        tints = [node for node in expressions if isinstance(node, u.MaterialExpressionVectorParameter)
                 and str(node.get_editor_property("parameter_name")) == "RoadTint"]
        require(len(textures) == len(tints) == 1, "Unexpected owned road texture/tint graph")
        road_tint = tints[0].get_editor_property("default_value")
        checks["owned_material_uses_planned_texture_tint"] = (asset(textures[0].get_editor_property("texture")) == plan["texture"]
            and close([road_tint.r, road_tint.g, road_tint.b, road_tint.a], plan["tint_rgba"], 0.000001))
        scalar_nodes = [node for node in expressions if isinstance(node, u.MaterialExpressionScalarParameter)]
        scalars = {str(node.get_editor_property("parameter_name")): node for node in scalar_nodes}
        expected_scalars = {"WorldUVScale": 0.002, "RoadOpacity": 0.9, "RoadRoughness": 0.9}
        scalar_values = {name: float(node.get_editor_property("default_value")) for name, node in scalars.items()}
        checks["owned_material_exact_scalars"] = (len(scalars) == 3 and set(scalars) == set(expected_scalars)
            and all(math.isfinite(scalar_values[name]) and abs(scalar_values[name] - value) <= 0.000001
                    for name, value in expected_scalars.items()))
        require(checks["owned_material_exact_scalars"], "Owned material scalar values/names mismatch")
        world_node = next(node for node in expressions if isinstance(node, u.MaterialExpressionWorldPosition))
        mask_node = next(node for node in expressions if isinstance(node, u.MaterialExpressionComponentMask))
        multiply_nodes = [node for node in expressions if isinstance(node, u.MaterialExpressionMultiply)]
        base_color = lib.get_material_property_input_node(material, u.MaterialProperty.MP_BASE_COLOR)
        opacity = lib.get_material_property_input_node(material, u.MaterialProperty.MP_OPACITY)
        roughness = lib.get_material_property_input_node(material, u.MaterialProperty.MP_ROUGHNESS)
        require(base_color in multiply_nodes and opacity == scalars["RoadOpacity"]
                and roughness == scalars["RoadRoughness"], "Owned material property output nodes mismatch")
        uv_node = next(node for node in multiply_nodes if node != base_color)

        def input_paths(node):
            return sorted(value.get_path_name() for value in lib.get_inputs_for_material_expression(material, node) if value is not None)

        expected_inputs = ((world_node, []), (mask_node, [world_node]),
            (scalars["WorldUVScale"], []), (uv_node, [mask_node, scalars["WorldUVScale"]]),
            (textures[0], [uv_node]), (tints[0], []), (base_color, [textures[0], tints[0]]),
            (scalars["RoadOpacity"], []), (scalars["RoadRoughness"], []))
        actual_inputs = {node.get_path_name(): input_paths(node) for node in expressions}
        mask_channels = [bool(mask_node.get_editor_property(channel)) for channel in ("r", "g", "b", "a")]
        checks["owned_material_exact_graph_inputs_outputs"] = (mask_channels == [True, True, False, False]
            and all(actual_inputs[node.get_path_name()] == sorted(value.get_path_name() for value in wanted)
                    for node, wanted in expected_inputs))
        report["owned_material_graph"] = {"class_counts": class_counts, "scalar_values": scalar_values,
            "mask_rgba": mask_channels, "inputs": actual_inputs,
            "outputs": {"BaseColor": base_color.get_path_name(), "Opacity": opacity.get_path_name(), "Roughness": roughness.get_path_name()}}
        guard()

        helper = u.Rogue10mCrop20261114WorldAuthoring
        terrain = json.loads(helper.inspect_candidate_terrain())
        snapshot = json.loads(helper.snapshot_retained_terrain_data())
        dependencies = json.loads(helper.inspect_candidate_dependencies())
        write("terrain", terrain)
        write("retained-height-paint-layers", snapshot)
        write("retained-dependencies", dependencies)
        checks["exact45_registered_lattice"] = (terrain.get("success") is True
            and terrain.get("retained_9_by_5_lattice_observed") is True and terrain.get("all_components_registered") is True
            and all(terrain.get(key) == 45 for key in ("component_count", "collision_component_count", "unique_component_count", "unique_collision_component_count")))
        checks["snapshot_complete"] = snapshot.get("success") is True and snapshot.get("all_requested_data_complete") is True
        for key in TERRAIN_KEYS:
            checks["terrain_preserved_" + key] = key in snapshot and key in baseline and snapshot[key] == baseline[key]
        checks["raw_BA_complete"] = dependencies.get("raw_ba_complete") is True
        checks["raw_BA_preserved"] = dependencies.get("raw_edit_layer_ba") == baseline_dependencies["raw_edit_layer_ba"]
        checks["raw_BA_metadata_preserved"] = dependencies.get("layer_metadata_digest") == baseline_dependencies["layer_metadata_digest"]
        checks["strict_loaded_reflection_complete"] = (dependencies.get("success") is True
            and dependencies.get("loaded_reflection_walk_complete") is True and dependencies.get("reference_scan_truncated") is False)
        for key in READ_ONLY_KEYS:
            checks["inspector_read_only_" + key] = dependencies.get(key) is True
        checks["dependency_scope_not_overstated"] = (dependencies.get("project_wide_dependency_complete") is False
            and dependencies.get("crop_reference_safety_verified") is False)
        write("actors", sorted(actual_rows, key=lambda row: row["path"]))

        protected = read(RESUME / "baseline-files.json")
        report["protected_inputs"] = protected_file_check(protected["protected_files"], ROOT)
        checks["protected_inputs_SHA256_unchanged"] = report["protected_inputs"]["passed"]
        saves = read(RESUME / "protected-save-files.json")
        save_root = ROOT / "Saved/SaveGames"
        report["protected_saves"] = protected_file_check(saves["files"], save_root)
        expected_names = {row["name"] for row in saves["files"]}
        observed_names = {path.name for path in save_root.glob("*.sav") if not permitted_qa_save(path.name)}
        report["protected_saves"].update(new_non_QA_A_files=sorted(observed_names - expected_names),
                                          absent_non_QA_A_files=sorted(expected_names - observed_names))
        checks["protected_non_QA_A_save_hashes_unchanged"] = report["protected_saves"]["passed"]
        checks["protected_non_QA_A_save_inventory_unchanged"] = observed_names == expected_names
        guard()
        report.update(status="passed_read_only_saved_data" if all(checks.values()) else "failed",
                      passed=all(checks.values()), dirty_after=dirty(), finished_utc=datetime.now(timezone.utc).isoformat())
        write("comparison", report)
        require(report["passed"], "Road-surface cold verification failed; preserve the report")
        print("LANDSCAPE_ROAD_SURFACE_COLD_20261117 PASS")
        return report
    except Exception:
        report.update(status="failed_preserve_evidence", passed=False, traceback=traceback.format_exc(), dirty_after=dirty())
        if not (OUT / "comparison.json").exists():
            write("comparison", report)
        raise
